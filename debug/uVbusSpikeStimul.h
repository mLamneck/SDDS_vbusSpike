class TvbusStimulSpike : public TvbusSpike485{
	private:
		int parseStr(uint8_t* _out, const char* _in){
		auto pEnd = _in + strlen(_in);
		auto pOutStart = _out;
		while (_in < pEnd) {
			_in += 2;
			dtypes::string byteString(_in,_in+2);
			uint8_t b = static_cast<uint8_t>(
				stoi(byteString, nullptr, 16));
			//Fps.writeByte(b);
			*_out++ = b;
			_in+=3;
		}
		return _out - pOutStart;
	}

	public:
		using TvbusSpike485::TvbusSpike485;

		Tevent FevStimul;

		TvbusStimulSpike(TmenuHandle& _root, Tuart* _stream) 
			: TvbusSpike485(_root,_stream)
		{
				FevStimul.setOwner(this);
				FevStimul.setPriority(2);
				FevStimul.signal();
		}

		void execute(Tevent* _ev) override{
			if (_ev == &FevStimul){
				stimul();
			}
			else{
				TvbusSpike485::execute(_ev);
			}
		};

		void stimul(){
				//static typename _Tstream::TmessageBufferRX FtxBuffer;
				static typename Tuart::TmessageBufferRX FtxBuffer;
				static int stimul_status = 0;
				dtypes::uint32 uint32Val = 0;
				switch(stimul_status){
					case 0:
						if (1==2){
							if (1==1){
								//Fps.init(&FtxBuffer.data[0]);
								//Fps.setHeader(0xFF,0xFF,0,TvbusProtocoll::dhcp_req);
								FtxBuffer.length = parseStr(&FtxBuffer.data[0],"0x00 0x01 0x02 0x00 0x25 0x02 0x02 0x03 0x01 0xF9 0x42 0x50 0x00 0x00 0x00");
								readMessage(&FtxBuffer);
							}

							Fps.init(&FtxBuffer.data[0]);
							Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::dhcp_set);
							Fps.writeByte(6);
							Fps.writeString("NUCLEO1");
							Fps.init(&FtxBuffer.data[0],Fps.length());
							Fdhcp.handleMessage(Fps);

							if (1 == 1){
								Fps.init(&FtxBuffer.data[0]);
								Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::ds_type_req);
								Fps.writeByte(13);	//client port
								Fps.writeByte(2);
								Fps.writeByte(0);
								Fps.writeByte(1);
								auto len = Fps.length();
								Fps.init(&FtxBuffer.data[0],Fps.length());
								handleMessage();
								Fps.init(&FtxBuffer.data[0],len);
								handleMessage();	
								stimul_status = 3;	
								return;				
							}

							for (int i=0; i<4; i++){
								Fps.init(&FtxBuffer.data[0]);
								Fps.setHeader(6,5,0,TvbusProtocoll::port_open);
								Fps.writeByte(1+i);//client port
								Fps.init(&FtxBuffer.data[0],Fps.length());
								handleMessage();
							}

							for (int i=0; i<4; i++){
								Fps.init(&FtxBuffer.data[0]);
								Fps.setHeader(6,5,0,TvbusProtocoll::port_open);
								Fps.writeByte(1+i);//client port
								Fps.init(&FtxBuffer.data[0],Fps.length());
								handleMessage();
							}

							//FPDW
							Fps.init(&FtxBuffer.data[0]);
							Fps.setHeader(6,5,0,TvbusProtocoll::ds_fpdw_req);
							Fps.writeByte(14); //client port
							Fps.writeByte(2);
							Fps.writeByte(0);
							Fps.writeByte(2);
							dtypes::uint32 val = 10;
							Fps.writeVal(val);
							Fps.init(&FtxBuffer.data[0],Fps.length());
							handleMessage();

							stimul_status = 1;
							FevStimul.setTimeEvent(10);

							//close it
							if (1==2){
								for (int i=0; i<=1; i++){
									Fps.init(&FtxBuffer.data[0]);
									Fps.setHeader(2,1,Connections::FIRST_PORT+i,TvbusProtocoll::port_close);
									Fps.writeByte(2-i);//client port
									Fps.init(&FtxBuffer.data[0],Fps.length());
									handleMessage();
								}
							}

							//Fstream->debugReadMessage(&FtxBuffer.data[0],Fps.length());
							//Fdhcp.handleMessage(FpsRead);
						}
						stimul_status = 1;

					//first give an ID=6 per dhcp set
					case 1:
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::dhcp_set);
						Fps.writeByte(1);
						Fps.writeString("NUCLEO1");
						Fps.init(&FtxBuffer.data[0],Fps.length());
						Fdhcp.handleMessage(Fps);
						stimul_status = 20;
						FevStimul.setTimeEvent(10);

						//open port
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(1,3,0,TvbusProtocoll::port_open);
						Fps.writeByte(1);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(1,3,0,TvbusProtocoll::port_open);
						Fps.writeByte(2);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(1,3,0x10,TvbusProtocoll::ds_link_req);
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(255);
						Fps.writeByte(0);//link time
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();

						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(1,3,0x11,TvbusProtocoll::ds_link_req);
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(255);
						Fps.writeByte(0);//link time
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();

						setTimeEvent(100);
						stimul_status = 2;
						break;
								
					case 2:
						//here I see no answer in real life
						//0x00 0xFF 0x02 0x00 0x06 0x00 0x00 0x0C 0x31 0x36 0x37 0x37 0x37 0x33 0x34 0x33 0x39 0x30 0x30 0x33 0x00 0x00
						FtxBuffer.length = parseStr(&FtxBuffer.data[0],"0x00 0x01 0x03 0x11 0x22 0x80 0x02 0x0C");
						readMessage(&FtxBuffer);
						break;


					//FPDW
					case 20:
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::ds_fpdw_req);
						Fps.writeByte(14); //client port
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(2);
						uint32Val = 10;
						Fps.writeVal(uint32Val);
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						stimul_status = 21;
						FevStimul.setTimeEvent(10);
						break;
					
					//type request
					case 21:{
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::ds_type_req);
						Fps.writeByte(13);	//client port
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(255);
						auto len = Fps.length();
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						stimul_status = 30;	
						break;
					}
					
					case 30:
						//message to set val to 12 that failed in reality
						 
						//link data
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,10,TvbusProtocoll::ds_link);
						Fps.writeByte(1);		//firstItemIdx;
						uint32Val = 15;
						Fps.writeVal(uint32Val);
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						break;
				}
		}
};