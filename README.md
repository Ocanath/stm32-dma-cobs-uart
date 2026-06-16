# DMA COBS UART
 
Source files for DMA-based UART receive and transmit on STM32, with COBS byte stuffing. Designed to be dropped into an STM32CubeIDE project as a linked source folder.
 
## Dependencies
 
Requires [dartt-protocol](https://github.com/ocanath/dartt-protocol) as a peer dependency — it must be submoduled and linked separately. The byte-stuffing library is bundled as a submodule of this repository.
 
## Linking via STM32CubeIDE

**IMPORTANT:** This library requires [dartt](https://github.com/ocanath/dartt-protocol) as a peer dependency in order to build. Submodule it and link it in you project.

1. Submodule this dependency and dartt.
2. Edit .project properties by adding the following (example):

```xml
	<linkedResources>
		<link>
			<name>Core/Src/C-COBS</name>
			<type>2</type>
			<locationURI>PROJECT_LOC/external/stm32-dma-cobs-uart/byte-stuffing/C-COBS</locationURI>
		</link>
		<link>
			<name>Core/Src/dartt</name>
			<type>2</type>
			<locationURI>PROJECT_LOC/external/dartt-protocol/src</locationURI>
		</link>
		<link>
			<name>Core/Src/stm32-dma-cobs-uart</name>
			<type>2</type>
			<locationURI>PROJECT_LOC/external/stm32-dma-cobs-uart/dma-uart-src</locationURI>
		</link>
	</linkedResources>
```
3. Edit the .cproject Release/ and Debug/ build configurations by injecting the paths in the xml properties. Each release configuration will have a reference to ../Core/Inc and the HAL drivers. Inject the dartt, cobs, and dma sources there. 

Example:

``` xml
	<listOptionValue builtIn="false" value="../external/stm32-dma-cobs-uart/byte-stuffing/C-COBS"/>
	<listOptionValue builtIn="false" value="../external/stm32-dma-cobs-uart/dma-uart-src"/>
	<listOptionValue builtIn="false" value="../external/dartt-protocol/src"/>
	<listOptionValue builtIn="false" value="../Core/Inc"/>
	<listOptionValue builtIn="false" value="../Drivers/STM32G4xx_HAL_Driver/Inc"/>
	<listOptionValue builtIn="false" value="../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"/>
	<listOptionValue builtIn="false" value="../Drivers/CMSIS/Device/ST/STM32G4xx/Include"/>
	<listOptionValue builtIn="false" value="../Drivers/CMSIS/Include"/>
```

Your project should now build.