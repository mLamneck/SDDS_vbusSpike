#include "uTestCase.h"
#include "uParamSave.h"
#include "../uVbusProtocol.h"
#include "../uDns.h"
#include "../uUartBase.h"

class TtestDns : public TtestCase{
    using TtestCase::TtestCase;

	//to be given as template parameters later
	typedef Tvbus485ProtStream TprotStream;
	typedef TuartBase _Tstream;
	typedef Tdns<TprotStream> Dns;

	TprotStream Fps;
	int FmessageLength = 0;
	typename _Tstream::TmessageBufferRX FtxBuffer;
	unitTest::TtestData FtestData;
	Dns Fdns;

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

	bool testRequest(int* _rootPath, const char* _textPath, int* _expPath){
		Fps.init(&FtxBuffer.data[0]);
		Fps.setHeader(5,6,0,TvbusProtocoll::dns_req);
		Fps.writeByte(7);	//client port
		TprotStream::t_path_length len = *_rootPath;
		Fps.writeVal(len);
		for (int i=0; i<len; i++){
			Fps.writePathEntry(_rootPath[i+1]);
		}

		Fps.writeString(_textPath,false);
		Fps.init(&FtxBuffer.data[0],Fps.length());
		Fdns.handleMessage(Fps,FtestData);
		FmessageLength = Fps.length();

		//toDo: check port, addr, ...
		Fps.init(&FtxBuffer.data[0],FmessageLength);
		if (Fps.func() != TvbusProtocoll::dns){
			debug::log("expecting func '0x%x' found '0x%x'",TvbusProtocoll::dns,Fps.func());
			return false;
		}

		return checkPath(_expPath);
	}

    bool test() override {
		constexpr int max_elements = 255;
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
		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {4,1,8,0,255};
			return testRequest(&rootPath[0],"allTypes/Fstr",&expResolvedPath[0]);
        },"root -> allTypes/Fstr");

		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {2,0,1};
			return testRequest(&rootPath[0],"var1",&expResolvedPath[0]);
        },"root -> var1");

        doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {2,0,1};
			return testRequest(&rootPath[0],"var1/",&expResolvedPath[0]);
        },"root -> var1/");

        doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {3,1,0,max_elements};
			return testRequest(&rootPath[0],"allTypes/",&expResolvedPath[0]);
        },"root -> allTypes");       
		
		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {3,1,0,max_elements};
			return testRequest(&rootPath[0],"allTypes/",&expResolvedPath[0]);
        },"root -> allTypes/");

		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {4,2,1,1,3};
			return testRequest(&rootPath[0],"sub/allTypes/Fuint16-Fint8",&expResolvedPath[0]);
        },"root -> sub/allTypes/Fuint16-Fint8");

		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {4,2,1,1,1};
			return testRequest(&rootPath[0],"sub/allTypes/Fuint16",&expResolvedPath[0]);
        },"root -> sub/allTypes/Fuint16");

		doTest([this](){
            int rootPath[] = {4,2,1,1,1};
            int expResolvedPath[] = {4,2,1,1,3};
			return testRequest(&rootPath[0],"-Fint8",&expResolvedPath[0]);
        },"sub/allTypes/Fuint16 -> -Fint8");

		doTest([this](){
            int rootPath[] = {2,0,max_elements};
            int expResolvedPath[] = {4,2,1,0,max_elements};
			return testRequest(&rootPath[0],"sub/allTypes",&expResolvedPath[0]);
        },"root -> sub/allTypes");

		doTest([this](){
            int rootPath[] = {4,2,1,0,max_elements};
            int expResolvedPath[] = {4,2,1,1,1};
			return testRequest(&rootPath[0],"/Fuint16",&expResolvedPath[0]);
        },"sub/allTypes -> /Fuint16");

		doTest([this](){
            int rootPath[] = {4,2,1,0,max_elements};
            int expResolvedPath[] = {4,2,1,1,3};
			return testRequest(&rootPath[0],"/Fuint16-Fint8",&expResolvedPath[0]);
        },"sub/allTypes -> /Fuint16-Fint8");

        return false;
    };
} dns_test("dns_test");
