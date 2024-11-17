#include "uTestCase.h"
#include "uParamSave.h"
#include "../src/uVbusProtocol.h"
#include "../src/uDataServer.h"
#include "../src/uUartBase.h"

class TtestDataServer : public TtestCase{
    using TtestCase::TtestCase;

	//to be given as template parameters later
	typedef Tvbus485ProtStream TprotStream;
	typedef TuartBase _Tstream;
	typedef TdataServer<TprotStream> TdataSever;
	typedef TprotStream::t_prot_msgCnt TmsgCnt;
	typedef TprotStream::t_path_length TpathLength;
	typedef TprotStream::t_path_entry TpathEntry;

	TprotStream Fps;
	int FmessageLength = 0;
	typename _Tstream::TmessageBufferRX FtxBuffer;
	unitTest::TtestData FtestData;
	TdataSever FdataServer;

	bool pathError(int* _expData){
		int expDataLen = *_expData;
		Fps.init(&FtxBuffer.data[0],FmessageLength);
		int len = Fps.bytesAvailableForRead();
		TprotStream::t_path_length resLen = 0;
		Fps.readVal(resLen);
		debug::write("path not correct \"%d",resLen);
		while (resLen-- > 0){
			TprotStream::t_path_entry e;
			if (!Fps.readVal(e)) break;
			debug::write(",%d",e);
		}
		debug::write("\" != \"%d",expDataLen);
		while (expDataLen-- > 0){
			debug::write(",%d",*++_expData);
		}
		debug::log("\" = expeceted path");

		return false;
	}

	bool checkPath(int* _expData){
		Fps.init(&FtxBuffer.data[0],FmessageLength);

		int expDataLen = *_expData;
		TprotStream::t_path_length len;
		if (!Fps.readVal(len)) return false;
		if (len != expDataLen)
			return pathError(_expData);

		for (int i = 0; i<expDataLen; i++){
			int c = _expData[i+1];
			TprotStream::t_path_entry e;
			if (!Fps.readVal(e)) return false;
			if (c != e){
				return pathError(_expData);
			}
		}

		debug::write("path resolved to ");
		debug::write("\"%d",expDataLen);
		while (expDataLen-- > 0){
			debug::write(",%d",*++_expData);
		}
		debug::log("\"");
		return true;
	}

	bool testTypeServerResponse(Tdescr* curr){
		TmsgCnt msgCnt;
		TpathEntry firstItem;
		sdds::typeIds::Ttype type;
		sdds::opt::Ttype option;

		Fps.init(&FtxBuffer.data[0],Fps.length());
		if (!Fps.readVal(msgCnt)){
			debug::log("no msgCnt");
			return false;
		};
		if (!Fps.readVal(firstItem)){
			debug::log("no firstItem");
			return false;			
		}			
		if (!Fps.readVal(type)){
			debug::log("no type");
			return false;			
		}
		if (!Fps.readVal(option)){
			debug::log("no option");
			return false;			
		}
		auto name = Fps.readString();
		if (type != curr->typeId() || option != curr->option() || !(name == curr->name())){
			debug::log("not expeceted type");
			return false;
		}
		else{
			debug::log("type for %s ok",curr->name());
		}
		return true;
	}

	bool testTypeRequest(int* _rootPath, Tdescr* _destStruct, Tdescr* _firstItem=nullptr, Tdescr* _lastItem=nullptr){
		Fps.init(&FtxBuffer.data[0]);
		Fps.setHeader(5,6,0,TvbusProtocoll::ds_type_req);
		Fps.writeByte(7);	//client port
		
		//write path
		TprotStream::t_path_length len = *_rootPath;
		Fps.writeVal(len);
		for (int i=0; i<len; i++){
			Fps.writePathEntry(_rootPath[i+1]);
		}

		//start typethread;
		FmessageLength = Fps.length();
		Fps.init(&FtxBuffer.data[0],FmessageLength);
		FdataServer.handleMessage(Fps,FtestData);

		//toDo: check for error answer;


		Tdescr* curr = _firstItem;
		if (!curr){
			auto s = static_cast<Tstruct*>(_destStruct)->value();
			curr = s->get(0);
		}
		if (!_lastItem){
			auto s = static_cast<Tstruct*>(_destStruct)->value();
			_lastItem = s->last();
		}
		
		if (_destStruct->isArray()){
			//toDo! check array response
			return testTypeServerResponse(_destStruct);
		}

		while (1==1){
			Fps.init(&FtxBuffer.data[0]);
			if (!FdataServer.FtypeST.Fevent.linked()){
				debug::log("types not completed");
				return false;
			}
			FdataServer.execute(&FdataServer.FtypeST.Fevent,Fps);
			if (!testTypeServerResponse(curr)) return false;;

			if (curr == _lastItem) break;
			curr = curr->next();
		};
		return true;
	}

	void testTypeRequests(){
		constexpr int max_elements = 255;

		doTest([this](){
            int path[] = {2,0,max_elements};
			return testTypeRequest(&path[0],&FtestData);
        },"typeRequest FtestData");

		doTest([this](){
            int path[] = {4,1,8,0,max_elements};
			return testTypeRequest(&path[0],&FtestData.allTypes.Fstr,&FtestData.allTypes.Fstr,&FtestData.allTypes.Fstr);
        },"typeRequest FtestData");

	}

    bool test() override {
		testTypeRequests();
		/* structure of TtestData
		var1=0
		-> allTypes
			Fuint8=0
			Fuint16=0
			Fuint32=0
			Fint8=0
			Fint16=0
			Fint32=0
			Ffloat32=0.000000
			Ftime=01.01.1970 10:00:00
			Fstr=
		<- allTypes
		-> sub
			Fuint8=246
			-> allTypes
				Fuint8=0
				Fuint16=0
				Fuint32=0
				Fint8=0
				Fint16=0
				Fint32=0
				Ffloat32=0.000000
				Ftime=01.01.1970 10:00:00
				Fstr=
			<- allTypes
		<- sub
		var2=251
		*/
        
		/* this test is newly added and for string elements. The expResolvedPath here
		 * is actually wrong. Strings should be treated like structs, so the path should be 4,1,8,0,255
		*/

        return false;
    };
} dataserver_test("dataServer_test");
