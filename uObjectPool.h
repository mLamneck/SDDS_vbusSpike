#ifndef UOBJECTPOOL_H
#define UOBJECTPOOL_H

template<class T, int _MAX_OBJECTS>
class TobjectPool{
	private:
		typedef T Tobjects[_MAX_OBJECTS];
		Tobjects Fobjects;

		/*
		class _Titerator{
			private:
				int Fidx = 0;
				Tobjects* Fobjs;
			public:
				_Titerator(){ }
				_Titerator(Tobjects* _objects){
					Fobjs = _objects;
					Fidx = 0;
				}

				int idx() { return Fidx - 1; }
		
				bool hasNext(){
					return (Fidx < _MAX_OBJECTS);
				}

				T* next(){
					return &((*Fobjs)[Fidx++]);
				}
		};
		*/

	public:
		constexpr static int MAX_OBJECTS = _MAX_OBJECTS;
		/*
		typedef _Titerator Titerator;

		_Titerator iterator(){
			Titerator it(&Fobjects);
			return it;
		}
		*/
		constexpr T* get(int _idx){ 
			if (_idx < 0 || _idx >= MAX_OBJECTS) return nullptr;
			return &Fobjects[_idx];
		}
};

#endif