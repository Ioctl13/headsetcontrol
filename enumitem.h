#include <stdint.h>

template<typename T>
struct EnumItem {
    T Enummeration;
    const uint8_t* Data;
    int Size;
};

template <typename T, int Size>
EnumItem<T>* GetEnumItemFromList(EnumItem<T>(&list)[Size], T enumeration)
{
    if(!list) return nullptr;
    for (int i = 0; i < Size; i++) {
        if(list[i].Enummeration == enumeration) {
            return &list[i];
        }
    }
    return nullptr;
}