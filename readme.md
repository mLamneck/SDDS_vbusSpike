# vbusSpike

`vbusSpike` is an extension of the [SDDS Library (Self-Describing Data Structures)](https://github.com/SDDS/sdds) that adds support for distributing data structures using a binary protocol. This library enables efficient communication and distribution of self-describing data structures in distributed systems and embedded applications.

Please note that this README is a work in progress. It currently provides basic definitions of the protocol, but a more detailed documentation will be added in the future.

## Features

- **Extension of SDDS:** Builds on top of the Self-Describing Data Structures (SDDS) Library for efficient data structure management.
- **Efficient Data Transfer:** Ensures low-latency and reliable data transfer for distributed systems.
- **Cross-Platform Compatibility:** Designed to work across different platforms and environments.

## Getting Started

### Prerequisites

To use `vbusSpike`, you will need:
- [SDDS Library](https://github.com/mLamneck/SDDS) installed.


## Documentation

### The Protocoll

#### The Protocoll - Header

| Ctrl		| Destiny | Source       	| Port  	| Func |
|----------	|-----------------	|------	| -| -

#### The Protocoll - Functions


|			| Function        	| Hex  	|
|----------	|-----------------	|------	|
|<b>Dhcp: 				 				|
|			| dhcp_queryReq   	| 0x01 	|
|			| dhcp_set		  	| 0x02 	|
|			| dhcp_req		  	| 0x03 	|
|			| dhcp_whoIsReq	  	| 0x05 	|
|			| dhcp_ka		  	| 0x06 	|
|			| dhcp_imServer   	| 0x08 	|
|<b>Connections:					 	|
|			| port_open		  	| 0x0A 	|
|			| port_close	  	| 0x0C 	|
|<b>Dns:				 				|
|			| dns_req		  	| 0x19 	|
|			| dns			  	| 0x18 	|
|<b>Dataserver:				 			|	
|			| ds_type			| 0x20 	|
|			| ds_type_req		| 0x21 	|
|			| ds_link			| 0x22 	|
|			| ds_type_req		| 0x23 	|
|			| ds_fpdw			| 0x24 	|
|			| ds_fpdw_req		| 0x25 	|
|			| ds_fpdr			| 0x26 	|
|			| ds_fpdr_req		| 0x27 	|
|<b>Error:				 			|	
|			| func_error		| 0xFE 	|


#### The Protocoll - Error Codes


| Error	        		| Hex  	|
|------------------		|------	|
| err_serverBusy		| 0xFF 	|
| err_invalidPort		| 0xFE 	|
| err_noMorePorts 		| 0xEB	|
| err_invalidPath 		| 0xFD	|
| err_invalidLinkTime 	| 0xF0	|


#### Connections

|				| Ctrl		| Destiny 	| Source    | Port 	| Func 	| Data		|
|-				| -			|- 		  	| -			| -		| -		| :- 		|
|<b>Req 		| AddrSize	| sAddr 	| cAddr		| 0		| 0x0A	| cPort	 	|
|<b>Ans 		| AddrSize	| cAddr 	| sAddr		| cPort	| 0x0B	| sPort		|

Possible Errors:
* err_noMorePorts

#### Dns

|				| Ctrl		| Destiny 	| Source    | Port 	| Func 	| Data								|
|-				| -			|- 		  	| -			| -		| -		| :- 								|
|<b>Req 		| AddrSize	| sAddr 	| cAddr		| 0		| 0x19	| cPort \| path 	\| textPath  	|
|<b>Ans 		| AddrSize	| cAddr 	| sAddr		| cPort	| 0x18	| path 								|

Possible Errors:
* err_invalidPath

#### Dataserver

#### Type Request

This is used to retrieve type information for a given binary path. A separate message will be sent for each variable, with the exception of enums. Enums may be split across multiple messages depending on their length. There is a 10ms delay between each message.

|				| Ctrl		| Destiny 	| Source    | Port 	| Func 	| Data								|
|-				| -			|- 		  	| -			| -		| -		| :- 								|
|<b>Req 		| AddrSize	| sAddr 	| cAddr		| 0		| 0x21	| cPort \| path		 				|
|<b>Ans 		| AddrSize	| cAddr 	| sAddr		| cPort	| 0x20	| msgCnt \| firstIdx \| **descr** 		|

where **descr** is the type description of the variable with firstIdx. The Format of **descr** is described in the next section.

Possible Errors:
* err_invalidPort
* err_invalidPath

#### Type Descr

The type descriptor constist of the following entries.
* **type:** a list of types can be found here.
* **option:** a list of possible options can be found here.
* **name:** a string with a leading lenght byte
* **[enumData]:** additional data for enums

In the table below there are some examples 

|				| type		| option 	| #name 	| name 	|[enumData]			|
|-				| -			|- 		  	| -		  	| -		|-					|
| uint16		| 0x02		| 0 		| 3			| var	|-					|
| int32			| 0x14		| 0 		| 5			| value |-
| enum			| 0x01		| 0 		| 4			| enum	| typeExt \| fullLen \| bufOfs \| enums		|

#### Enum Data in Type Descr

For Enum variables there is additional information:
* <b>typeExt:</b> is a byte for future reserved. At the moment it's always 0
* <b>fullLen:</b> length of all enums concate
* <b>bufPos:</b> first char in the complete buffer
* **enumData** array of pascal strings.

If the typeinformation needs to be split in multiple messages, only the first one contains the name field. In subsequent messages the name field is 0 ("").

Example of for a data transmission for an enum at position 10 (idx=9) in a struct with the name **enum2** (05 65 6E 75 6D 32) with the following entries. All values in HEX.
| enum | hex representation | 
| -|-|
|on | 02 6F 6E
| off | 03 6F 66 66
| idle| 04 69 64 6C 65
| heat| 04 68 65 61 74
| cool| 04 63 6F 6F 6C

|nThMsg		|firstIdx	|	type/opt/name			| typeExt	| fullLength	|	strOfs	| enumData 	|
|-			|-			| -							| -			| -				|	- 		| -			|
|80 		|09		 	| 01 00 05 65 6E 75 6D 32	| 00		| 16			| 00 		| 02 6F 6E 03 6F 66 66 04 69 64 6C 65 04 68 65 61 74 04 63 6F 6F 6C |

To be finished...

#### Link Request

|				| Ctrl		| Destiny 	| Source    | Port 	| Func 	| Data								|
|-				| -			|- 		  	| -			| -		| -		| :- 								|
|<b>Req 		| AddrSize	| sAddr 	| cAddr		| sPort	| 0x23	| path 	\| binary data 				|
|<b>Ans 		| AddrSize	| cAddr 	| sAddr		| cPort	| 0x22	| msgCnt \| firstIdx \| binary data |

Possible Errors:
* err_invalidPort
* err_invalidPath

#### Full Path Data Write Request

|			| Ctrl		| Destiny 	| Source    | Port 	| Func 	| 		 	Data					|
|-			| -			|- 		  	| -			| -		| -		| :- 								|
|<b>Req 	| AddrSize	| sAddr 	| cAddr		|0		| 0x26	| cPort \| path 	\| binary data 	|
|<b>Ans 	| AddrSize	| cAddr 	| sAddr		|5		| 0x27	| -									|		

Possible Errors:
* err_invalidPath



## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any changes you would like to make.

## Acknowledgments

- The [SDDS Library](https://github.com/SDDS/sdds) team for providing the foundational self-describing data structures.
- Special thanks to all contributors and the open-source community.
