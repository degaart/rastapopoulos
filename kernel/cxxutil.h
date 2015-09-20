/*
    Reimplementation of certain utilities from the STL
 */
#ifndef _CXXUTIL_H_
#define _CXXUTIL_H_

template <class _Tp> struct remove_reference_        {typedef _Tp type;};
template <class _Tp> struct remove_reference_<_Tp&>  {typedef _Tp type;};
template <class _Tp> struct remove_reference_<_Tp&&> {typedef _Tp type;};
template <class _Tp> using Remove_reference = typename remove_reference_<_Tp>::type;

template<typename T> Remove_reference<T>&& move(T&& t) noexcept {
    return static_cast<Remove_reference<T>&&>(t);
}

template<typename T>
void swap(T& a, T& b) noexcept {
    T tmp {move(a)};
    a = move(b);
    b = move(tmp);
}

#endif
