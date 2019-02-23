#ifndef RIME_LUA_TEMPLATES_H_
#define RIME_LUA_TEMPLATES_H_

#include <rime/common.h>
#include <typeinfo>
#include "lua.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#if LUA_VERSION_NUM >= 502
#define xlua_resume(L, a) lua_resume(L, NULL, a)
#define LUA_UNPACK "table.unpack"
#else
#define xlua_resume lua_resume
#define lua_rawlen lua_objlen
#define LUA_UNPACK "unpack"
#endif

namespace rime {
//--- LuaType
// Generic case (includes pointers)
template<typename T>
struct LuaType {
  static const string name() {
    return boost::typeindex::type_id_with_cvr<T>().pretty_name();
    // return string("V") + typeid(T).name();
  }

  static int gc(lua_State *L) {
    T *o = (T *) luaL_checkudata(L, 1, name().c_str());
    o->~T();
    return 0;
  }

  static void pushdata(lua_State *L, T o) {
    void *u = lua_newuserdata(L, sizeof(T));
    new(u) T(o);
    luaL_getmetatable(L, name().c_str());
    if (lua_isnil(L, -1)) {
      // If T is not registered,
      // registers a "__gc" to prevent memory leaks.
      lua_pop(L, 1);
      luaL_newmetatable(L, name().c_str());
      lua_pushstring(L, "__gc");
      lua_pushcfunction(L, gc);
      lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
  }

  static T todata(lua_State *L, int i) {
    typedef typename std::remove_const<T>::type U;
    T *o = (T *) luaL_testudata(L, i, name().c_str());
    if (o)
      return *o;

    if (std::is_const<T>::value) {
      T *o = (T *) luaL_testudata(L, i, LuaType<U>::name().c_str());
      if (o)
        return *o;
    }

    const char *msg = lua_pushfstring(L, "%s expected", name().c_str());
    luaL_argerror(L, i, msg);
    return *((T *) NULL);
  }
};

// References
template<typename T>
struct LuaType<T &> {
  static const string name() {
    return boost::typeindex::type_id_with_cvr<T &>().pretty_name();
    // return string("R") + typeid(T).name();
  }

  static void pushdata(lua_State *L, T &o) {
    T **u = (T**) lua_newuserdata(L, sizeof(T *));
    *u = &o;
    luaL_setmetatable(L, name().c_str());
  }

  static T &todata(lua_State *L, int i) {
    typedef typename std::remove_const<T>::type U;
    T **po = (T **) luaL_testudata(L, i, name().c_str());
    if (po)
      return **po;

    if (std::is_const<T>::value) {
      T **po = (T **) luaL_testudata(L, i, LuaType<U &>::name().c_str());
      if (po)
        return **po;
    }

    T *o = (T *) luaL_testudata(L, i, LuaType<T>::name().c_str());
    if (o)
      return *o;

    if (std::is_const<T>::value) {
      T *o = (T *) luaL_testudata(L, i, LuaType<U>::name().c_str());
      if (o)
        return *o;
    }

    const char *msg = lua_pushfstring(L, "%s expected", name().c_str());
    luaL_argerror(L, i, msg);
    return *((T *) NULL);
  }
};

// Optional
template<typename T>
struct LuaType<optional<T>> {
  static void pushdata(lua_State *L, optional<T> o) {
    if (o)
      LuaType<T>::pushdata(L, *o);
    else
      lua_pushnil(L);
  }
};

// Bool
template<>
struct LuaType<bool> {
  static void pushdata(lua_State *L, bool o) {
    lua_pushboolean(L, o);
  }

  static bool todata(lua_State *L, int i) {
    return lua_toboolean(L, i);
  }
};

// Integers
template<>
struct LuaType<int> {
  static void pushdata(lua_State *L, lua_Integer o) {
    lua_pushinteger(L, o);
  }

  static lua_Integer todata(lua_State *L, int i) {
    return luaL_checkinteger(L, i);
  }
};

template<>
struct LuaType<unsigned int> : LuaType<int> {};

template<>
struct LuaType<long> : LuaType<int> {};

template<>
struct LuaType<unsigned long> : LuaType<int> {};

template<>
struct LuaType<long long> : LuaType<int> {};

template<>
struct LuaType<unsigned long long> : LuaType<int> {};

template<>
struct LuaType<short> : LuaType<int> {};

template<>
struct LuaType<unsigned short> : LuaType<int> {};

template<>
struct LuaType<char> : LuaType<int> {};

template<>
struct LuaType<unsigned char> : LuaType<int> {};

// Floats
template<>
struct LuaType<double> {
  static void pushdata(lua_State *L, lua_Number o) {
    lua_pushnumber(L, o);
  }

  static lua_Number todata(lua_State *L, int i) {
    return luaL_checknumber(L, i);
  }
};

template<>
struct LuaType<float> : LuaType<double> {};

template<>
struct LuaType<long double> : LuaType<double> {};

// Strings
template<>
struct LuaType<string> {
  static void pushdata(lua_State *L, const string &o) {
    lua_pushstring(L, o.c_str());
  }

  static string todata(lua_State *L, int i) {
    return string(luaL_checkstring(L, i));
  }
};

template<>
struct LuaType<const string> : LuaType<string> {};

template<>
struct LuaType<const string &> : LuaType<string> {};

/* string references/pointers are not supported now */

// Arrays
// The index starts form 1 in Lua...
template<typename T>
struct LuaType<vector<T>> {
  static void pushdata(lua_State *L, const vector<T> &o) {
    int n = o.size();
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; i++) {
      LuaType<T>::pushdata(L, o[i]);
      lua_rawseti(L, -2, i + 1);
    }
  }
  static vector<T> todata(lua_State *L, int j) {
    vector<T> o;
    int n = lua_rawlen(L, j);
    for (int i = 0; i < n; i++) {
      lua_rawgeti(L, j, i + 1);
      o.push_back(LuaType<T>::todata(L, -1));
      lua_pop(L, 1);
    }
    return o;
  }
};

template<typename T>
struct LuaType<const vector<T>> : LuaType<vector<T>> {};

template<typename T>
struct LuaType<const vector<T> &> : LuaType<vector<T>> {};

// Helper function for push multiple data
static void pushdataX(lua_State *L) {}

template<typename T>
static void pushdataX(lua_State *L, T o) {
  LuaType<T>::pushdata(L, o);
}

template<typename T, typename ... Targs>
static void pushdataX(lua_State *L, T o, Targs ... oargs) {
  LuaType<T>::pushdata(L, o);
  pushdataX<Targs ...>(L, oargs ...);
}

namespace LuaImpl {
  int thread_stub(lua_State *);
}

// --- Lua call/resume
template <typename ... I>
int Lua::newthread(const string &f, I ... input) {
  lua_getglobal(L_, f.c_str());
  pushdataX<I ...>(L_, input ...);
  return newthread(L_, sizeof...(input));
}

template <typename O>
optional<O> Lua::resume(int id) {
  lua_rawgeti(L_, LUA_REGISTRYINDEX, id);
  lua_State *C = lua_tothread(L_, -1);
  lua_pop(L_, 1);

  int status = xlua_resume(C, 0);
  if (status == LUA_YIELD) {
    O c = LuaType<O>::todata(C, -1);
    lua_pop(C, 1);
    return c;
  } else {
    if (status != LUA_OK) {
      const char *e = lua_tostring(C, -1);
      printf("resume(err=%d): %s\n", status, e);
    }
    return {};
  }
}


// --- LuaWrapper
// WRAP(f): wraps function f
// WRAPT(f, R, T...): wraps function f (R: return type, T...: argument types)
// WRAPMEM(C', C::f): wraps member function C::f  (object wraped by C')
// WRAPMEM_GET/SET(C', C::f): wraps member variable C::f (object wraped by C')

template<typename F, F f>
struct LuaWrapper;

// LuaWrapper: R(*)(T...) -> int (*)(lua_State *)
template<typename S, typename... T, S(*f)(T...)>
struct LuaWrapper<S(*)(T...), f> {
  // args = R
  template<typename R, typename... A>
  struct args {
    template<typename... Us>
    struct aux {
      template<int n, typename Is_void>
      struct ret {
        static int wrap(lua_State *L, Us... us) {
          R r = f(us...);
          LuaType<R>::pushdata(L, r);
          return 1;
        }
      };

      template<int n>
      struct ret<n, void> {
        static int wrap(lua_State *L, Us... us) {
          f(us...);
          return 0;
        }
      };

      template<int n, typename Rp>
      struct ret<n, Rp *> {
        static int wrap(lua_State *L, Us... us) {
          Rp *r = f(us...);
          if (r)
            LuaType<Rp *>::pushdata(L, r);
          else
            lua_pushnil(L);
          return 1;
        }
      };

      template<int n, typename Rp>
      struct ret<n, an<Rp>> {
        static int wrap(lua_State *L, Us... us) {
          an<Rp> r = f(us...);
          if (r)
            LuaType<an<Rp>>::pushdata(L, r);
          else
            lua_pushnil(L);
          return 1;
        }
      };

      template<int n>
      static int wrap(lua_State *L, Us... us) {
        return ret<n, R>::wrap(L, us...);
      }
    };
  };

  // args = R :: A :: As
  template<typename R, typename A, typename... As>
  struct args<R, A, As...> {
    template<typename... Us>
    struct aux {
      template <int n>
      static int wrap(lua_State *L, Us... us) {
        A t = LuaType<A>::todata(L, n);
        return args<R, As...>::
          template aux<Us..., A &>::
          template wrap<n + 1>(L, us..., t);
      }
    };
  };

  template<typename... X>
  static int wrapt(lua_State *L) {
    return args<X...>::
      template aux<>::
      template wrap<1>(L);
  }

  static int wrap(lua_State *L) {
    return wrapt<S, T...>(L);
  }
};

// MemberWrapper: R (C::*)(T..) -> R (*)(C *, T...) / R (C &, T...) / R (an<C>, T...)
// MemberWrapper(get variable): R (C::*) -> R (*)(C *) / R (C &) / R (an<C>)
// MemberWrapper(set variable): R (C::*) -> void (*)(C *, R) / void (C &, R) / void (an<C>, R)
template<typename D, typename F, F f>
struct MemberWrapper;

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...)>
struct MemberWrapper<D *, R (C::*)(T...), f> {
  static R wrap(D *c, T... t) {
    return (c->*f)(t...);
  }
};

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...)>
struct MemberWrapper<D, R (C::*)(T...), f> {
  static R wrap(D &c, T... t) {
    return (c.*f)(t...);
  }
};

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...)>
struct MemberWrapper<an<D>, R (C::*)(T...), f> {
  static R wrap(an<D> ac, T... t) {
    C *c = ac.get();
    return (c->*f)(t...);
  }
};

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...) const>
struct MemberWrapper<D *, R (C::*)(T...) const, f> {
  static R wrap(D *c, T... t) {
    return (c->*f)(t...);
  }
};

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...) const>
struct MemberWrapper<D, R (C::*)(T...) const, f> {
  static R wrap(const D &c, T... t) {
    return (c.*f)(t...);
  }
};

template<typename D, typename R, typename C, typename... T, R (C::*f)(T...) const>
struct MemberWrapper<an<D>, R (C::*)(T...) const, f> {
  static R wrap(an<D> ac, T... t) {
    C *c = ac.get();
    return (c->*f)(t...);
  }
};

template<typename D, typename R, typename C, R C::*f>
struct MemberWrapper<D *, R (C::*), f> {
  static R wrap_get(D *c) {
    return c->*f;
  }

  static void wrap_set(D *c, R r) {
    c->*f = r;
  }
};

template<typename D, typename R, typename C, R C::*f>
struct MemberWrapper<D, R (C::*), f> {
  static R wrap_get(const D &c) {
    return c.*f;
  }

  static void wrap_set(D &c, R r) {
    c.*f = r;
  }
};

template<typename D, typename R, typename C, R C::*f>
struct MemberWrapper<an<D>, R (C::*), f> {
  static R wrap_get(an<D> ac) {
    C *c = ac.get();
    return c->*f;
  }

  static void wrap_set(an<D> ac, R r) {
    C *c = ac.get();
    c->*f = r;
  }
};

#define WRAP(f) (&(LuaWrapper<decltype(&f), &f>::wrap))
#define WRAPT(f, ...) (&(LuaWrapper<decltype(&f), &f>::wrapt<__VA_ARGS__>))
#define WRAPMEM(C, f) (&(LuaWrapper<decltype(&MemberWrapper<C, decltype(&f), &f>::wrap), \
                                             &MemberWrapper<C, decltype(&f), &f>::wrap>::wrap))
#define WRAPMEM_GET(C, f) (&(LuaWrapper<decltype(&MemberWrapper<C, decltype(&f), &f>::wrap_get), \
                                                 &MemberWrapper<C, decltype(&f), &f>::wrap_get>::wrap))
#define WRAPMEM_SET(C, f) (&(LuaWrapper<decltype(&MemberWrapper<C, decltype(&f), &f>::wrap_set), \
                                                 &MemberWrapper<C, decltype(&f), &f>::wrap_set>::wrap))
}
#endif /* RIME_LUA_TEMPLATES_H_ */
