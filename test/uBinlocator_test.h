#include "uTestCase.h"
#include "uParamSave.h"
#include "../uVbusProtocol.h"
#include "../uDns.h"
#include "../uUart.h"
#include "uTypedef.h"

class TbinLocatorTest : public TtestCase{
    using TtestCase::TtestCase;

	//to be given as template parameters later
	typedef Tvbus485ProtStream TprotStream;
	typedef Tuart _Tstream;
	typedef Tdns<TprotStream> Dns;

	TprotStream Fps;
	typename _Tstream::TmessageBufferRX FtxBuffer;
	unitTest::TtestData FtestData;

	bool testPath(int* _reqPath, bool _expRes, Tdescr* _expParent, Tdescr* _expFirst, Tdescr* _expLast){
		TbinLocator<TprotStream::t_path_length,TprotStream::t_path_entry> l;
		Fps.init(&FtxBuffer.data[0]);
		TprotStream::t_path_length len = *_reqPath;
		Fps.writeVal(len);
		for (int i=0; i<len; i++){
			Fps.writePathEntry(_reqPath[i+1]);
		}
		Fps.init(&FtxBuffer.data[0],Fps.length());
		auto res = l.locate(Fps,FtestData);
		if (res != _expRes){
			debug::log("expected result = %d != %d",_expRes,res);
			return false;
		}

		if (!res) return true;
		
		if (l.parent() != _expParent){
			debug::log("parent is not the expected '%s'",_expParent->name());
			return false;
		}

		res = true;
		if (l.firstItem() != _expFirst){
			debug::log("firstItem = '%s' != '%s' = expected firstItem",l.firstItem()->name(),_expFirst->name());
			res = false;
		}

		if (l.lastItem() != _expLast){
			debug::log("lastItem = '%s' != '%s' = expected lastItem",l.lastItem()->name(),_expLast->name());
			res = false;
		}

		return res;
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
        
		doTest([this](){
            int reqPath[] = {2,0,max_elements};
			return testPath(&reqPath[0],true,FtestData,FtestData.get(0),FtestData.last());
        },"2,0,255");

		doTest([this](){
            int reqPath[] = {3,1,0,max_elements};
			return testPath(&reqPath[0],true,FtestData.allTypes,FtestData.allTypes.get(0),FtestData.allTypes.last());
        },"3,1,0,255");

		doTest([this](){
            int reqPath[] = {3,1,1,2};
			return testPath(&reqPath[0],true,&FtestData.allTypes,FtestData.allTypes.get(1),FtestData.allTypes.get(1+2-1));
        },"3,1,1,2");

		doTest([this](){
            int reqPath[] = {4,1,8,0,255};
			auto strDescr = FtestData.allTypes.get(8);
			return testPath(&reqPath[0],true,&FtestData.allTypes.Fstr,strDescr,strDescr);
        },"enter string 4,1,8,0,255");

        return false;
    };
} binLocator_test("binLocator_test");
