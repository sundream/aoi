## 介绍
* aoi的九宫格和十字链表两种实现

## 状态
* 尚未经过线上项目验证

## API
* 见grid/src/aoi.h或者crosslink/src/aoi.h
* 另外lua-bind相关见grid/lua-bind/laoi.c或者crosslink/lua-bind/laoi.c

## 编译和运行
以九宫格实现为例说明,十字链表实现编译方式完全相同
* c源码:
	* 编译: cd grid/src && make clean && make all
	* 运行: ./aoi

* lua绑定: 
	* 编译: cd grid/lua-bind && make clean && make all
	* 运行: lua test.lua

## 用法
* 见各目录下的test.c/test.lua

## 优缺点
* 九宫格
	* 优点: cpu消耗小
	* 缺点: 内存开销大,内存消耗不仅和实体数有关,还和场景大小成正比

* 十字链表
	* 优点: 内存开销小,内存消耗仅和实体数有关,和场景大小无关
	* 缺点: cpu消耗高,每次移动都需要计算视野差,当实体在小区域堆积严重时效率更差

## 参考
* [aoi](https://github.com/cloudwu/aoi)
* [十字链表实现](http://github.com/lichuang/AOI)
