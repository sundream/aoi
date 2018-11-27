/**
 * @module laoi.c
 * @author sundream
 * @release 0.0.1 @ 2018/11/26
 */

#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>
#include "aoi.h"

typedef struct lua_aoi_space {
	aoi_space *aoi;
	int cb_enterAOI;
	int cb_leaveAOI;
	lua_State *L;
} lua_aoi_space;

static void
enterAOI(void *ud,uint32_t watcher,uint32_t marker) {
	lua_aoi_space *laoi = ud;
	lua_State *L = laoi->L;
	lua_rawgeti(L,LUA_REGISTRYINDEX,LUA_RIDX_MAINTHREAD);
	lua_State *mL = lua_tothread(L,-1);
	lua_pop(L,1);
	lua_rawgeti(mL,LUA_REGISTRYINDEX,laoi->cb_enterAOI);
	lua_pushlightuserdata(mL,laoi);
	lua_pushinteger(mL,watcher);
	lua_pushinteger(mL,marker);
	lua_pcall(mL,3,0,0);
}

static void
leaveAOI(void *ud,uint32_t watcher,uint32_t marker) {
	lua_aoi_space *laoi = ud;
	lua_State *L = laoi->L;
	lua_rawgeti(L,LUA_REGISTRYINDEX,LUA_RIDX_MAINTHREAD);
	lua_State *mL = lua_tothread(L,-1);
	lua_pop(L,1);
	lua_rawgeti(mL,LUA_REGISTRYINDEX,laoi->cb_leaveAOI);
	lua_pushlightuserdata(mL,laoi);
	lua_pushinteger(mL,watcher);
	lua_pushinteger(mL,marker);
	lua_pcall(mL,3,0,0);
}

static int
laoi_gc(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	if (laoi->cb_enterAOI != LUA_NOREF) {
		luaL_unref(L,LUA_REGISTRYINDEX,laoi->cb_enterAOI);
	}
	if (laoi->cb_leaveAOI != LUA_NOREF) {
		luaL_unref(L,LUA_REGISTRYINDEX,laoi->cb_leaveAOI);
	}
	if (laoi->aoi != NULL) {
		aoi_release(laoi->aoi);
	}
	return 0;
}

/**
 * 新建一个AOI对象
 * @function laoi.new
 * @param map_x 地图长度(x轴)
 * @param map_y 地图宽度(y轴)
 * @param map_z 地图高度(z轴)
 * @param tower_x 对于九宫格: 灯塔长度,对于十字链表: x轴方向视野半径
 * @param tower_y 对于九宫格: 灯塔宽度,对于十字链表: y轴方向视野半径
 * @param tower_z 对于九宫格: 灯塔高度,对于十字链表: z轴方向视野半径
 * @param cb_enterAOI 进入AOI回调函数
 * @param cb_leaveAOI 离开AOI回调函数
 * @return AOI对象
 */
static int
laoi_new(lua_State *L) {
	float map_size[3];
	float tower_size[3];
	map_size[0] = luaL_checknumber(L,1);
	map_size[1] = luaL_checknumber(L,2);
	map_size[2] = luaL_checknumber(L,3);
	tower_size[0] = luaL_checknumber(L,4);
	tower_size[1] = luaL_checknumber(L,5);
	tower_size[2] = luaL_checknumber(L,6);
	if (lua_gettop(L) != 8) {
		return luaL_argerror(L,0,"invalid argument");
	}
	luaL_checktype(L,-1,LUA_TFUNCTION);
	luaL_checktype(L,-2,LUA_TFUNCTION);
	int cb_leaveAOI = luaL_ref(L,LUA_REGISTRYINDEX);
	int cb_enterAOI = luaL_ref(L,LUA_REGISTRYINDEX);
	lua_aoi_space *laoi = lua_newuserdata(L,sizeof(*laoi));
	laoi->L = L;
	laoi->cb_enterAOI = cb_enterAOI;
	laoi->cb_leaveAOI = cb_leaveAOI;
	luaL_getmetatable(L,"laoi_meta");
	lua_setmetatable(L,-2);
	aoi_space *aoi = aoi_new(map_size,tower_size,enterAOI,leaveAOI,laoi);
	laoi->aoi = aoi;
	return 1;
}

/*
static int
laoi_release(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	aoi_release(laoi->aoi);
	return 0;
}
*/

/**
 * 增加一个实体
 * @function aoi:enter
 * @param id 实体ID
 * @param x 实体x坐标
 * @param y 实体y坐标
 * @param z 实体z坐标
 * @param modestring 实体模式:w(atcher)--观察者,m(arker)--被观察者.
 * 如"wm"表示该实体即为观察者又为被观察者,内部实现AOI事件只会通知观察者
 */
static int
laoi_enter(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	uint32_t id = luaL_checkinteger(L,2);
	float pos[3];
	int i;
	for (i=0; i<3; i++) {
		pos[i] = luaL_checknumber(L,3+i);
	}
	const char *mode = luaL_checkstring(L,6);
	aoi_enter(laoi->aoi,id,pos,mode);
	return 0;
}

/**
 * 删除一个实体
 * @function aoi:leave
 * @param id 实体ID
 */
static int
laoi_leave(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	uint32_t id = luaL_checkinteger(L,2);
	aoi_leave(laoi->aoi,id);
	return 0;
}

/**
 * 移动实体(更新实体坐标)
 * @function aoi:move
 * @param id 实体ID
 * @param x 实体x坐标
 * @param y 实体y坐标
 * @param z 实体z坐标
 */
static int
laoi_move(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	uint32_t id = luaL_checkinteger(L,2);
	float pos[3];
	int i;
	for (i=0; i<3; i++) {
		pos[i] = luaL_checknumber(L,3+i);
	}
	aoi_move(laoi->aoi,id,pos);
	return 0;
}

/**
 * 更新实体模式
 * @function aoi:change_mode
 * @param id 实体ID
 * @param modestring 实体模式:w(atcher)--观察者,m(arker)--被观察者.
 * 如"wm"表示该实体即为观察者又为被观察者,内部实现AOI事件只会通知观察者
 */
static int
laoi_change_mode(lua_State *L) {
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	uint32_t id = luaL_checkinteger(L,2);
	const char *mode = luaL_checkstring(L,3);
	aoi_change_mode(laoi->aoi,id,mode);
	return 0;
}

/**
 * 根据位置获取视野范围内的实体
 * @function aoi:get_view_by_pos
 * @param x 位置x坐标
 * @param y 位置y坐标
 * @param z 位置z坐标
 * @param range_x 范围x大小
 * @param range_y 范围y大小
 * @param range_z 范围z大小
 *		(过滤的空间是以指定位置为中心,范围为半径表示的立方体,
 *		范围不传时,对于九宫格实现: 表示获取灯塔周围九宫格范围,对于十字链表实现: 则使用默认视野半径大小)
 * @return 实体ID列表
 */
static int
laoi_get_view_by_pos(lua_State *L) {
	int i;
	float pos[3];
	float range[3];
	bool has_range = false;
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	for (i=0; i<3; i++) {
		pos[i] = luaL_checknumber(L,2+i);
	}
	if (lua_gettop(L) > 4) {
		has_range = true;
		for (i=0; i<3; i++) {
			range[i] = luaL_checknumber(L,5+i);
		}
	}
	int number = 0;
	void **ids;
	if (has_range) {
		ids = aoi_get_view_by_pos(laoi->aoi,pos,range,&number);
	} else {
		ids = aoi_get_view_by_pos(laoi->aoi,pos,NULL,&number);
	}
	lua_createtable(L,number,0);
	for(i=0; i<number; i++) {
		lua_pushinteger(L,(uint32_t)ids[i]);
		lua_rawseti(L,-2,i+1);
	}
	return 1;
}

/**
 * 根据实体所在位置获取视野范围内的实体
 * @function aoi:get_view
 * @param id 实体ID
 * @param range_x 范围x大小
 * @param range_y 范围y大小
 * @param range_z 范围z大小
 *		(过滤的空间是以实体的位置为中心,范围为半径表示的立方体,
 *		范围不传时,对于九宫格实现: 表示获取灯塔周围九宫格范围,对于十字链表实现: 则使用默认视野半径大小)
 * @return 实体ID列表
 */
static int
laoi_get_view(lua_State *L) {
	int i;
	float range[3];
	bool has_range = false;
	lua_aoi_space *laoi = lua_touserdata(L,1);
	luaL_argcheck(L,laoi != NULL,1,"Need a aoi object");
	uint32_t id = luaL_checkinteger(L,2);
	if (lua_gettop(L) > 2) {
		has_range = true;
		for (i=0; i<3; i++) {
			range[i] = luaL_checknumber(L,3+i);
		}
	}
	int number = 0;
	void **ids;
	if (has_range) {
		ids = aoi_get_view(laoi->aoi,id,range,&number);
	} else {
		ids = aoi_get_view(laoi->aoi,id,NULL,&number);
	}
	lua_createtable(L,number,0);
	for(i=0; i<number; i++) {
		lua_pushinteger(L,(uint32_t)ids[i]);
		lua_rawseti(L,-2,i+1);
	}
	return 1;
}

LUAMOD_API int
luaopen_laoi(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg laoi_methods[] = {
		{"enter",laoi_enter},
		{"leave",laoi_leave},
		{"move",laoi_move},
		{"change_mode",laoi_change_mode},
		{"get_view_by_pos",laoi_get_view_by_pos},
		{"get_view",laoi_get_view},
		{NULL,NULL},
	};

	luaL_Reg l[] = {
		{"new",laoi_new},
		{"enter",laoi_enter},
		{"leave",laoi_leave},
		{"move",laoi_move},
		{"change_mode",laoi_change_mode},
		{"get_view_by_pos",laoi_get_view_by_pos},
		{"get_view",laoi_get_view},
		{NULL,NULL},
	};

	luaL_newmetatable(L,"laoi_meta");
	lua_newtable(L);
	luaL_setfuncs(L,laoi_methods,0);
	lua_setfield(L,-2,"__index");
	lua_pushcfunction(L,laoi_gc);
	lua_setfield(L,-2,"__gc");

	luaL_newlib(L,l);
	return 1;
}
