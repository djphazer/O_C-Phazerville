/*
 * A collection of type factories and applet pointers for lazy initialization.
 * Started with example code from ChatGPT.
 */

#include <memory>
#include <type_traits>

using RegID = uint64_t;

// --- Declare an applet for the Registry ---
template<typename T, RegID Id, uint8_t Categories>
struct DeclareApplet {
    using type = T;
    static constexpr size_t size = sizeof(T);
    static constexpr RegID id = Id;
    static constexpr uint8_t categories = Categories;
};

template<typename T>
struct DeclareFancyApplet {
    using type = T;
    static constexpr size_t size = sizeof(T);
    static constexpr RegID id = strhash(type::applet_name_());
    static constexpr uint8_t categories = 0;
};

// --- Duplicate ID check ---
template<RegID... Ids>
struct NoDuplicateIDs;

template<>
struct NoDuplicateIDs<> : std::true_type {};

template<RegID First, RegID... Rest>
struct NoDuplicateIDs<First, Rest...>
    : std::bool_constant<((First != Rest) && ...) && NoDuplicateIDs<Rest...>::value> {};

// --- Registry ---
template <class T, size_t Slots, typename... Declarations>
struct Registry {
    using FactoryFn = T* (*)();
    static constexpr size_t Size = sizeof...(Declarations);

    // Compile-time build of factories array
    static constexpr std::array<FactoryFn, Size> buildFactories() {
        std::array<FactoryFn, Size> arr{
          (+[]() -> T* {
            void* block = (OC::CORE::FreeRam() > OC::CORE::RAM2_HEADROOM) ? calloc(1, Declarations::size) : nullptr;
            if (!block) block = extmem_calloc(1, Declarations::size);
            if (block) return new (block) typename Declarations::type();
            return nullptr;
           }) ...
        };
        return arr;
    }

    static constexpr std::array<FactoryFn, Size> factories = buildFactories();

    mutable std::array<std::array<T*, Size>, Slots> instances{}; // Raw pointers, default nullptr

    constexpr Registry() {
        static_assert(NoDuplicateIDs<Declarations::id...>::value,
                      "Duplicate Applet IDs detected in Registry!");
        // TODO: compiler doesn't like this syntax?
        //(static_assert(Declarations::id <= MaxID, "Applet ID exceeds MaxID for Registry"), ...);
    }

    static constexpr std::array<RegID, sizeof...(Declarations)> getIds() {
      std::array<RegID, sizeof...(Declarations)> arr{ Declarations::id ... };
      return arr;
    }

    static constexpr std::array<uint8_t, sizeof...(Declarations)> getCategories() {
      return {Declarations::categories ...};
    }
    static constexpr std::array<const char *, sizeof...(Declarations)> getNames() {
      return {Declarations::type::applet_name_() ...};
    }
    static constexpr std::array<const uint8_t*, sizeof...(Declarations)> getIcons() {
      return {Declarations::type::applet_icon_() ...};
    }

    const char* getName(int index) const {
      return getNames()[index];
    }
    const uint8_t* getIcon(int index) const {
      return getIcons()[index];
    }

    T* get(RegID id, size_t slot = 0) const {
        int idx = -1;
        auto Ids = getIds();
        for (size_t i = 0; i < Size; ++i) {
          if (id == Ids[i]) {
            idx = i;
            break;
          }
        }
        if (idx < 0 || !factories[idx]) {
            return nullptr;
        }
        if (!instances[slot][idx]) {
            Serial.printf("Free RAM: %d\n", OC::CORE::FreeRam());
            Serial.printf("AppletRegistry: new - ID: %u Index: %d Slot: %u\n", id, idx, slot);
            instances[slot][idx] = factories[idx]();
            Serial.println(instances[slot][idx]->applet_name());
        }
        return instances[slot][idx];
    }
};
