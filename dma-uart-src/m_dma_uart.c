/*
 * m_uart.c
 *
 *  Created on: May 17, 2021
 *      Author: Ocanath
 */
#include "m_dma_uart.h"
// #include "uart_buffers.h"
// #include "dartt_mctl_params.h"

/* Flag to clear ALL uart-associated interrupt requests, without clobbering reserved bits
 * (1 << 20) | (1 << 17) | (1 << 12) | (1 << 11) | (1 << 9) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0)
 */
#define ICR_CLEAR_ALL	0x00121BDF

/*ISR bits*/
#define RXNE_BIT 	(1 << 5)
#define TXE_BIT		(1 << 7)
#define IDLE_BIT	(1 << 4)

/*CR1 bits*/
#define TXEIE		(1 << 7)



/**/
void m_uart_start_interrupts(dma_uart_t * h)
{
//	h->Instance->CR1 |= (1 << 5) | (1 << 7) | (1 << 2) | (1 << 3);       //enable rxneie, txeie, RE and TE
//	h->Instance->CR1 &= ~(1 << 7);       //disable TX interrupt
//	h->Instance->CR1 |= (1 << 4);        //enable IDLE interrupt

	//setup interrupts and UART config
	h->Instance->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
	h->Instance->CR3 |= USART_CR3_DMAR;
	h->Instance->CR3 |= USART_CR3_DMAT;

	//setup rxdma
	h->rxdma->CCR &= ~DMA_CCR_EN;	//disable dma (will often already be disabled. Necessary for writing to CNTR, etc.
	h->rxdma->CCR |= DMA_CCR_CIRC;
	h->rxdma->CNDTR = h->rx_mem.size;
	h->rxdma->CPAR = (uint32_t)(&h->Instance->RDR);
	h->rxdma->CMAR = (uint32_t)(&h->rx_mem.buf[0]);
	h->rxdma->CCR |= DMA_CCR_TCIE;
	h->rxdma->CCR |= DMA_CCR_EN;

	//setup txdma
	h->txdma->CCR &= ~DMA_CCR_EN;	//disable the dma for writing configuration info
	h->txdma->CNDTR = 0;
	h->txdma->CPAR = (uint32_t)(&h->Instance->TDR);
	h->txdma->CMAR = (uint32_t)(&h->tx_mem.buf[0]);
	h->txdma->CCR |= DMA_CCR_TCIE;	//enable transfer complete interrupt (and just tc interrupt - no others)
	h->txdma->CCR |= DMA_CCR_EN;	//re-enable the dma

}

/*
 * Assign memory aliases and initalize interrupts
 */
int init_dma_uart(dma_uart_t * h,
		USART_TypeDef * Instance,
		DMA_Channel_TypeDef * rxdma,
		DMA_Channel_TypeDef * txdma,
		unsigned char * rx_encoded_mem,
		size_t rx_encoded_mem_size,
		unsigned char * rx_decoded_mem,
		size_t rx_decoded_mem_size,
		unsigned char * tx_mem,
		size_t tx_mem_size)
{
	if(h == NULL ||
		Instance == NULL ||
		rxdma == NULL ||
		txdma == NULL ||
		rx_encoded_mem == NULL ||
		rx_decoded_mem == NULL ||
		tx_mem == NULL)
	{
		return -1;
	}
	h->Instance = Instance;
	h->rxdma = rxdma;
	h->txdma = txdma;
	h->rx_mem = (cobs_buf_t){
		.buf = rx_encoded_mem,
		.size =  rx_encoded_mem_size,
		.length = 0,
		.encoded_state = COBS_ENCODED
	};
	h->rx_decoded = (cobs_buf_t){
			.buf = rx_decoded_mem,
			.size = rx_decoded_mem_size,
			.length = 0,
			.encoded_state = COBS_DECODED
	};
	h->rx_decode_alias = (dartt_buffer_t){
			.buf = rx_decoded_mem,
			.size = rx_decoded_mem_size,
			.len = 0
	};
	h->rx_pld_msg = (payload_layer_msg_t){};
	h->tx_mem = (cobs_buf_t){
			.buf = tx_mem,//gl_tx_mem,
			.size = tx_mem_size,
			.length = 0,
			.encoded_state = COBS_DECODED
	};
	h->tx_buf_alias = (dartt_buffer_t){
			.buf = tx_mem,
			.size = tx_mem_size,
			.len = 0
	};
	m_uart_start_interrupts(h);
	return 0;
}


/**/
void m_uart_disable_rx_interrupt(dma_uart_t * h)
{
	h->Instance->CR1 &= ~USART_CR1_RXNEIE;
}

/**/
void m_uart_enable_rx_interrupt(dma_uart_t * h)
{
	h->Instance->CR1 |= USART_CR1_RXNEIE;
}

/*
 * Baremetal uart handler.
 *
 * Note: may require timer to trigger based on rx activity, to reset if partial frame detected. Depends on the behavior of the IDLE interrupt in
 * edge cases.
 *
 * Idea: simultaneously do PPP unstuffing
 * */
void m_uart_it_handler(dma_uart_t * h)
{
	uint16_t rdr = (uint16_t)h->Instance->RDR;	//read RDR, thus clearing the associated interrupt flag
	if(rdr == 0)	//rxne will always be zero, because the DMA clears the FIFO. That means we don't care about the state of that bit - we only need to check RDR, or alternatively the most recent value in DMA memory
	{
		h->rx_mem.length = (h->rx_mem.size - (size_t)h->rxdma->CNDTR);	//load length based on dma register status. It counts down so we just reverse it from the known transfer size
		//reset the dma pointer back to zero. we received a COBS frame, so everything preceeding is irrelevant.
		h->rxdma->CCR &= ~DMA_CCR_EN;
		h->rxdma->CNDTR = h->rx_mem.size;	//may need to frame disable/enable
		h->rxdma->CCR |= DMA_CCR_EN;
		cobs_decode_double_buffer(&h->rx_mem, &h->rx_decoded);
		h->rx_decode_alias.len = h->rx_decoded.length; //dumb, but we have to copy the length because we have a dartt buffer and cobs buffer. Should really do something to unify these..
	}

	h->Instance->ICR |=  ICR_CLEAR_ALL;	//clear all remaining interrupt flags to avoid a storm
}


//read dma interface for how many bytes to send remain
uint32_t m_uart_bytes_to_send(dma_uart_t * h)
{
	return h->txdma->CNDTR;
}

/**
 * DMA Transmit function.
 * Load pointers to memory, length, and enable it
 */
int m_uart_dma_transmit(dma_uart_t * h)
{
	if(h == NULL)
	{
		return ERROR_UART_BAD_INPUT;
	}
	if(h->tx_mem.buf == NULL)
	{
		return ERROR_UART_BAD_INPUT;
	}
	h->txdma->CCR &= ~DMA_CCR_EN;
	h->txdma->CMAR = (uint32_t)(&h->tx_mem.buf[0]);	//ensure pointer is updated. tx_mem.buf might have changed.
	h->txdma->CNDTR = h->tx_mem.length;	//ensure length is loaded - might have changed.
	h->txdma->CCR |= DMA_CCR_EN;
	return SUCCESS_UART;
}


/*m_uart receive dma handler*/
void m_uart_rxdma_handler(DMA_HandleTypeDef *hdma)
{
    hdma->DmaBaseAddress->IFCR = ((uint32_t)DMA_ISR_GIF1 << (hdma->ChannelIndex & 0x1FU));	//global per-channel interrupt clear
}

/*m_uart transmit dma handler*/
void m_uart_txdma_handler(DMA_HandleTypeDef *hdma)
{
	hdma->DmaBaseAddress->IFCR = ((uint32_t)DMA_ISR_GIF1 << (hdma->ChannelIndex & 0x1FU));	//global per-channel interrupt clear
}

