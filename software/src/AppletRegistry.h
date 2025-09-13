/*
 * A collection of type factories and applet pointers for lazy initialization.
 * Started with example code from ChatGPT.
 */

#include <memory>
#include <type_traits>

// --- Declare an applet for the Registry ---
template<typename T, uint16_t Id, uint8_t Categories>
struct DeclareApplet {
    using type = T;
    static constexpr uint16_t id = Id;
    static constexpr uint8_t categories = Categories;
};

// --- Duplicate ID check ---
template<uint16_t... Ids>
struct NoDuplicateIDs;

template<>
struct NoDuplicateIDs<> : std::true_type {};

template<uint16_t First, uint16_t... Rest>
struct NoDuplicateIDs<First, Rest...>
    : std::bool_constant<((First != Rest) && ...) && NoDuplicateIDs<Rest...>::value> {};

// --- Registry ---
template <class T, std::size_t MaxID, typename... Declarations>
struct Registry {
    using ID = uint16_t;
    using FactoryFn = T* (*)();

    // Compile-time build of factories array
    static constexpr std::array<FactoryFn, MaxID + 1> buildFactories() {
        std::array<FactoryFn, MaxID + 1> arr{};
        ((arr[Declarations::id] = +[]() -> T* { return new typename Declarations::type(); }), ...);
        return arr;
    }

    static constexpr std::array<FactoryFn, MaxID + 1> factories = buildFactories();

    mutable std::array<std::array<T*, MaxID + 1>, HS::APPLET_SLOTS> instances{}; // Raw pointers, default nullptr

    constexpr Registry() {
        static_assert(NoDuplicateIDs<Declarations::id...>::value,
                      "Duplicate Applet IDs detected in Registry!");
        // TODO: compiler doesn't like this syntax?
        //(static_assert(Declarations::id <= MaxID, "Applet ID exceeds MaxID for Registry"), ...);
    }

    static constexpr std::array<uint16_t, sizeof...(Declarations)> getIds() {
      std::array<uint16_t, sizeof...(Declarations)> arr{ Declarations::id ... };
      return arr;
    }

    // TODO:
    //static constexpr std::array<const char *, sizeof...(Declarations)> getNames() {
      //return {Declarations::type::applet_name ...};
    //}

    const char* getName(ID id) const {
      // TODO: make names static
      return "[name]";
    }
    const uint8_t* getIcon(ID id) const {
      // TODO: make icons static
      return ZAP_ICON;
    }

    T* get(ID id, HS::HEM_SIDE slot = 0) const {
        if (id > MaxID || !factories[id]) {
            return nullptr;
        }
        if (!instances[slot][id]) {
            instances[slot][id] = factories[id]();
        }
        return instances[slot][id];
    }
};
