/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: lwip_netif.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 07 月 30 日
**
** 描        述: lwip 网口计数器改进.
                 lwip netif_add() 总是增加网络接口计数器, 但 netif_remove() 并没有处理.
                 
** BUG:
2011.02.13  netif_remove_hook() 中加入对 npf detach 的操作, 确保 attach 与 detach 成对操作.
2011.03.10  将 _G_ulNetIfLock 开放, posix net/if.h 需要此锁.
2011.07.04  加入对路由表的回调操作.
2013.04.16  netif_remove_hook 需要卸载 DHCP 数据结构.
2013.09.24  移除网络接口加入对 auto ip 的回收.
2014.03.22  可以通过索引号, 快速得到 netif.
*********************************************************************************************************/
#define  __SYLIXOS_RTHOOK
#define  __SYLIXOS_KERNEL
#define  __NETIF_MAIN_FILE
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/err.h"
#include "lwip_if.h"
#include "net/if.h"
#if LW_CFG_NET_ROUTER > 0
#include "net/route.h"
#include "route/af_route.h"
#include "route/ip4_route.h"
#include "route/ip6_route.h"
#if LW_CFG_NET_BALANCING > 0
#include "net/sroute.h"
#include "balancing/ip4_sroute.h"
#include "balancing/ip6_sroute.h"
#endif                                                                  /*  LW_CFG_NET_BALANCING > 0    */
#endif                                                                  /*  LW_CFG_NET_ROUTER > 0       */
#if LW_CFG_NET_MROUTER > 0
#include "mroute/ip4_mrt.h"
#endif                                                                  /*  LW_CFG_NET_MROUTER > 0      */
#if LW_CFG_NET_FLOWCTL_EN > 0
#include "flowctl/net_flowctl.h"
#endif
/*********************************************************************************************************
  网络接口数量宏定义
*********************************************************************************************************/
#define __LW_NETIF_MAX_NUM      LW_CFG_NET_DEV_MAX
#define __LW_NETIF_USED         1
#define __LW_NETIF_UNUSED       0
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static struct netif *_G_pnetifArray[__LW_NETIF_MAX_NUM];
static UINT          _G_uiNetifNum = 0;
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
#if LW_CFG_NET_NPF_EN > 0
VOID  __npfNetifRemoveHook(struct netif  *pnetif);
#endif                                                                  /*  LW_CFG_NET_NPF_EN > 0       */
/*********************************************************************************************************
** 函数名称: netif_add_hook
** 功能描述: 创建网络接口回调函数, 返回网络接口号 (网络上下文中调用)
** 输　入  : pvNetif     网络接口
** 输　出  : ERR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  netif_add_hook (PVOID  pvNetif)
{
    struct netif  *netif = (struct netif *)pvNetif;
    INT            i;
    
    if (_G_ulNetifLock == 0) {
        _G_ulNetifLock =  API_SemaphoreMCreate("netif_lock", LW_PRIO_DEF_CEILING, 
                                               LW_OPTION_WAIT_PRIORITY |
                                               LW_OPTION_DELETE_SAFE |
                                               LW_OPTION_INHERIT_PRIORITY |
                                               LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    }
    
    LWIP_NETIF_LOCK();                                                  /*  进入临界区                  */
    for (i = 0; i < __LW_NETIF_MAX_NUM; i++) {
        if (_G_pnetifArray[i] == LW_NULL) {
            _G_pnetifArray[i] =  netif;
            netif->num        =  (UINT8)i;
            _G_uiNetifNum++;
            break;
        }
    }
    LWIP_NETIF_UNLOCK();                                                /*  退出临界区                  */
    
    if (i >= __LW_NETIF_MAX_NUM) {
        return  (ERR_USE);
    }
    
#if LW_CFG_NET_ROUTER > 0
    rt_netif_add_hook(netif);                                           /*  更新路由表有效标志          */
#if LWIP_IPV6
    rt6_netif_add_hook(netif);
#endif
#if LW_CFG_NET_BALANCING > 0
    srt_netif_add_hook(netif);
#if LWIP_IPV6
    srt6_netif_add_hook(netif);
#endif
#endif                                                                  /*  LW_CFG_NET_BALANCING > 0    */
    route_hook_netif_ann(netif, 0);
#endif                                                                  /*  LW_CFG_NET_ROUTER > 0       */
#if LW_CFG_NET_FLOWCTL_EN > 0
    //fcnet_netif_attach(netif);
#endif                                                                  /*  LW_CFG_NET_FLOWCTL_EN > 0   */
    
    return  (ERR_OK);
}
/*********************************************************************************************************
** 函数名称: netif_remove_hook
** 功能描述: 删除网络接口回调函数. (网络上下文中调用)
** 输　入  : pvNetif     网络接口
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  netif_remove_hook (PVOID  pvNetif)
{
    struct netif  *netif = (struct netif *)pvNetif;
    INT            iNum  = (INT)netif->num;
    
#if LW_CFG_NET_ROUTER > 0
    rt_netif_remove_hook(netif);                                        /*  更新路由表有效标志          */
#if LWIP_IPV6
    rt6_netif_remove_hook(netif);
#endif
#if LW_CFG_NET_BALANCING > 0
    srt_netif_remove_hook(netif);
#if LWIP_IPV6
    srt6_netif_remove_hook(netif);
#endif
#endif                                                                  /*  LW_CFG_NET_BALANCING > 0    */
#if LW_CFG_NET_MROUTER > 0
    ip4_mrt_if_detach(netif);
#endif                                                                  /*  LW_CFG_NET_MROUTER > 0      */
    route_hook_netif_ann(netif, 1);
#endif                                                                  /*  LW_CFG_NET_ROUTER > 0       */
#if LW_CFG_NET_FLOWCTL_EN > 0
    //fcnet_netif_detach(netif);
#endif                                                                  /*  LW_CFG_NET_FLOWCTL_EN > 0   */
    
    if (iNum < __LW_NETIF_MAX_NUM) {
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        _G_pnetifArray[iNum] = LW_NULL;
        _G_uiNetifNum--;
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
    }
    
#if LW_CFG_NET_NPF_EN > 0
    __npfNetifRemoveHook(netif);
#endif                                                                  /*  LW_CFG_NET_NPF_EN > 0       */

#if LWIP_DHCP > 0
    if (netif_dhcp_data(netif)) {
        dhcp_stop(netif);                                               /*  关闭 DHCP 回收 UDP 控制块   */
        dhcp_cleanup(netif);                                            /*  回收 DHCP 内存              */
    }
#endif                                                                  /*  LWIP_DHCP > 0               */

#if LWIP_AUTOIP > 0
    if (netif_autoip_data(netif)) {
        mem_free(netif_autoip_data(netif));                             /*  回收 AUTOIP 内存            */
        netif_set_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_AUTOIP, NULL);
    }
#endif                                                                  /*  LWIP_AUTOIP > 0             */
}
/*********************************************************************************************************
** 函数名称: netif_updown_hook
** 功能描述: 网络接口使能禁能回调.
** 输　入  : pvNetif    网络接口
**           up         1: 使能 0: 禁能
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_updown_hook (PVOID  pvNetif, INT up)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    if (!up) {
        rt_netif_invcache_hook(netif);
#if LWIP_IPV6
        rt6_netif_invcache_hook(netif);
#endif
    }
    route_hook_netif_updown(netif, up);
#endif
}
/*********************************************************************************************************
** 函数名称: netif_link_updown_hook
** 功能描述: 网络接口 link 连接状态回调.
** 输　入  : pvNetif    网络接口
**           linkup     1: 连接 0: 禁能
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_link_updown_hook (PVOID  pvNetif, INT linkup)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    if (!linkup) {
        rt_netif_invcache_hook(netif);
#if LWIP_IPV6
        rt6_netif_invcache_hook(netif);
#endif
    }
#endif
}
/*********************************************************************************************************
** 函数名称: netif_set_addr_hook
** 功能描述: 网络接口设置地址.
** 输　入  : pvNetif    网络接口
**           pvIpaddr   新地址
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_set_addr_hook (PVOID  pvNetif, const PVOID pvIpaddr)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    route_hook_netif_ipv4(netif, (ip4_addr_t *)pvIpaddr);
#endif
}
/*********************************************************************************************************
** 函数名称: netif_set_addr6_hook
** 功能描述: 网络接口设置地址.
** 输　入  : pvNetif    网络接口
**           pvIpaddr   新地址
**           iIndex     地址下标
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_set_addr6_hook (PVOID  pvNetif, const PVOID pvIp6addr, INT iIndex)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    const ip6_addr_t  *pip6addr;

    pip6addr = netif_ip6_addr(netif, iIndex);
    if (!ip6_addr_isany(pip6addr)) {
        route_hook_netif_ipv6(netif, pip6addr, RTM_DELADDR);
    }
    
    pip6addr = (ip6_addr_t *)pvIp6addr;
    if (!ip6_addr_isany(pip6addr)) {
        route_hook_netif_ipv6(netif, pip6addr, RTM_NEWADDR);
    }
#endif
}
/*********************************************************************************************************
** 函数名称: netif_set_maddr_hook
** 功能描述: 网络接口 添加 / 删除 组播地址.
** 输　入  : pvNetif    网络接口
**           pvIpaddr   新地址
**           iAdd       是否为添加
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_set_maddr_hook (PVOID  pvNetif, const PVOID pvIpaddr, INT iAdd)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    route_hook_maddr_ipv4(netif, (ip4_addr_t *)pvIpaddr, (u_char)(iAdd ? RTM_NEWMADDR : RTM_DELMADDR));
#endif
}
/*********************************************************************************************************
** 函数名称: netif_set_addr6_hook
** 功能描述: 网络接口 添加 / 删除 组播地址.
** 输　入  : pvNetif    网络接口
**           pvIpaddr   新地址
**           iIndex     地址下标
**           iAdd       是否为添加
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID netif_set_maddr6_hook (PVOID  pvNetif, const PVOID pvIp6addr,  INT iAdd)
{
    struct netif  *netif = (struct netif *)pvNetif;
    
#if LW_CFG_NET_ROUTER > 0
    route_hook_maddr_ipv6(netif, (ip6_addr_t *)pvIp6addr, (u_char)(iAdd ? RTM_NEWMADDR : RTM_DELMADDR));
#endif
}
/*********************************************************************************************************
** 函数名称: netif_get_num
** 功能描述: 获得网络接口数量.
** 输　入  : NONE
** 输　出  : 网络接口数量
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT netif_get_num (VOID)
{
    return  (_G_uiNetifNum);
}
/*********************************************************************************************************
** 函数名称: netif_get_by_index
** 功能描述: 通过 index 获得网络接口结构. (没有加锁)
** 输　入  : uiIndex       index
** 输　出  : 网络接口
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PVOID netif_get_by_index (UINT uiIndex)
{
    if (uiIndex < __LW_NETIF_MAX_NUM) {
        return  (_G_pnetifArray[uiIndex]);
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: netif_get_flags
** 功能描述: 通过 index 获得网络接口结构. (没有加锁)
** 输　入  : uiIndex       index
** 输　出  : 网络接口
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  netif_get_flags (struct netif *pnetif)
{
    INT  iFlags = 0;

    if (pnetif->flags & NETIF_FLAG_UP) {
        iFlags |= IFF_UP;
    }
    if (pnetif->flags & NETIF_FLAG_BROADCAST) {
        iFlags |= IFF_BROADCAST;
    } else {
        iFlags |= IFF_POINTOPOINT;
    }
    if (pnetif->flags & NETIF_FLAG_LINK_UP) {
        iFlags |= IFF_RUNNING;
    }
    if (pnetif->flags & NETIF_FLAG_IGMP) {
        iFlags |= IFF_MULTICAST;
    }
    if ((pnetif->flags & NETIF_FLAG_ETHARP) == 0) {
        iFlags |= IFF_NOARP;
    }
    if (netif_ip4_addr(pnetif)->addr == ntohl(INADDR_LOOPBACK)) {
        iFlags |= IFF_LOOPBACK;
    }
    if ((pnetif->flags2 & NETIF_FLAG2_PROMISC)) {
        iFlags |= IFF_PROMISC;
    }
    if ((pnetif->flags2 & NETIF_FLAG2_ALLMULTI)) {
        iFlags |= IFF_ALLMULTI;
    }
    
    return  (iFlags);
}

#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
