#pragma once
//
// template <class B, class... Cs, size_t... Is>
// static constexpr std::array<B *, sizeof(Cs...)>
// tuple_to_ptr_arr(std::tuple<Cs...> tuple) {
//   return std::array<B *, sizeof...(Is)>{&std::get<Is>()}
// }

namespace util {
// static constexpr std::array<B *, sizeof...(Cs)>

template <class B, class... Cs>
static constexpr std::array<B *, sizeof...(Cs)>
tuple_to_ptr_arr(std::tuple<Cs...> &tuple) {
  return std::array<B *, sizeof...(Cs)>{&std::get<Cs>(tuple)...};
}
// tuple_arrs_to_ptr_arrs(std::tuple<Cs..., size_t Ns...> tuple) {
//   return std::array<B *, sizeof...(Cs)>{&std::get<Cs>(tuple)...};
// }

} // namespace util
