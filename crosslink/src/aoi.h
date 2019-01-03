/**
 * @module aoi.h
 * @author sundream
 * @release 0.0.1 @ 2018/11/26
 */

#ifndef aoi_h
#define aoi_h
#include <stdint.h>

typedef void * (*aoi_Alloc)(void *ud, void * ptr, size_t sz);
typedef void (*enterAOI_Callback)(void *ud,uint32_t watcher, uint32_t marker);
typedef void (*leaveAOI_Callback)(void *ud,uint32_t watcher, uint32_t marker);


typedef struct aoi_space aoi_space;
/**
 * 创建一个AOI对象
 * @function aoi_create
 * @param alloc 内存分配函数
 * @param alloc_ud 内存分配函数的用户数据
 * @param map_size 地图大小(依次对应为x,y,z轴)
 * @param tower_size 九宫格实现:灯塔大小,十字链表实现:视野半径大小(x,y,z轴3个方向)
 * @param cb_enterAOI 进入AOI回调函数
 * @param cb_leaveAOI 离开AOI回调函数
 * @param cb_ud	进入/离开AOI回调时透传的用户数据
 * @return AOI对象
 */
aoi_space *aoi_create(aoi_Alloc alloc,void *alloc_ud,float map_size[3],float tower_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud);
/**
 * 创建一个AOI对象,用默认的内存分配函数调用aoi_create实现
 * @function aoi_new
 * @param map_size 地图大小(依次对应为x,y,z轴)
 * @param tower_size 九宫格实现:灯塔大小,十字链表实现:视野半径大小(x,y,z轴3个方向)
 * @param cb_enterAOI 进入AOI回调函数
 * @param cb_leaveAOI 离开AOI回调函数
 * @param cb_ud	进入/离开AOI回调时透传的用户数据
 * @return AOI对象
 */
aoi_space *aoi_new(float map_size[3],float tower_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud);
/**
 * 释放一个aoi对象
 * @function aoi_release
 * @param aoi AOI对象
 */
void aoi_release(aoi_space *aoi);
/**
 * 增加一个实体
 * @function aoi_enter
 * @param aoi AOI对象
 * @param id 添加到场景的实体ID,由上层管理，需要保证唯一
 * @param pos 位置
 * @param modestring 实体模式:w(atcher)--观察者,m(arker)--被观察者.
 * 如"wm"表示该实体即为观察者又为被观察者,内部实现AOI事件只会通知观察者
 *
 */
void aoi_enter(aoi_space *aoi,uint32_t id,float pos[3],const char *modestring);
/**
 * 删除一个实体
 * @function aoi_leave
 * @param aoi AOI对象
 * @param id 实体ID
 */
void aoi_leave(aoi_space *aoi,uint32_t id);
/**
 * 移动实体(更新实体坐标)
 * @function aoi_move
 * @param aoi AOI对象
 * @param id 实体ID
 * @param pos 位置
 */
void aoi_move(aoi_space *aoi,uint32_t id,float pos[3]);
/**
 * 更新实体模式
 * @function aoi_change_mode
 * @param aoi AOI对象
 * @param id 实体ID
 * @param modestring 实体模式:w(atcher)--观察者,m(arker)--被观察者.
 */
void aoi_change_mode(aoi_space *aoi,uint32_t id,const char *modestring);
/**
 * 根据位置获取视野范围内的实体
 * @function aoi_get_view_by_pos
 * @param aoi AOI对象
 * @param pos 位置
 * @param range 范围
 *		(过滤的空间是以pos为中心,range为半径表示的立方体,
 *		range为空时,对于九宫格实现: 表示获取灯塔周围九宫格范围,对于十字链表实现: 则使用默认视野半径大小)
 * @param number [out] 返回的实体数量
 * @return 实体ID列表
 */
void **aoi_get_view_by_pos(aoi_space *aoi,float pos[3],float range[3],int *number);
/**
 * 根据实体所在位置获取视野范围内的实体
 * @function aoi_get_view_by_pos
 * @param aoi AOI对象
 * @param id 实体ID
 * @param range 范围
 *		(过滤的空间是以指定实体坐标为中心,range为半径表示的立方体,
 *		range为空时,对于九宫格实现: 表示获取灯塔周围九宫格范围,对于十字链表实现: 则使用默认视野半径大小)
 * @param number [out] 返回的实体数量
 * @return 实体ID列表
 */
void **aoi_get_view(aoi_space *aoi,uint32_t id,float range[3],int *number);


#endif
