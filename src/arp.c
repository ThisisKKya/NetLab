#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type = swap16(ARP_HW_ETHER),
    .pro_type = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = DRIVER_IF_IP,
    .sender_mac = DRIVER_IF_MAC,
    .target_mac = {0}};
/**
 * @brief arp地址转换表
 * 
 */
arp_entry_t arp_table[ARP_MAX_ENTRY];
/**
 * @brief 长度为1的arp分组队列，当等待arp回复时暂存未发送的数据包
 * 
 */
arp_buf_t arp_buf;

/**
 * @brief 更新arp表
 *        你首先需要依次轮询检测ARP表中所有的ARP表项是否有超时，如果有超时，则将该表项的状态改为无效。
 *        接着，查看ARP表是否有无效的表项，如果有，则将arp_update()函数传递进来的新的IP、MAC信息插入到表中，
 *        并记录超时时间，更改表项的状态为有效。
 *        如果ARP表中没有无效的表项，则找到超时时间最长的一条表项，
 *        将arp_update()函数传递进来的新的IP、MAC信息替换该表项，并记录超时时间，设置表项的状态为有效。
 * 
 * @param ip ip地址
 * @param mac mac地址
 * @param state 表项的状态
 */
void arp_update(uint8_t *ip, uint8_t *mac, arp_state_t state)
{
    // TODO
    time_t now = time(NULL);
    time_t min = 0;
    int minindex; 
    int updateflag = 0;//0 is not updated yet  1 is updated
    for(int i =0;i<ARP_MAX_ENTRY;i++)
    {
        if(arp_table[i].state == ARP_INVALID)
        {
            if (updateflag == 0)//有超时且为更新
            {
                strcpy(arp_table[i].ip,ip);
                strcpy(arp_table[i].mac,mac);
                arp_table[i].state = ARP_VALID;
                arp_table[i].timeout = now+ARP_TIMEOUT_SEC;
                updateflag = 1;
            }
            continue;
        }
        if(arp_table[i].timeout < now)//if timeout
        {
            arp_table[i].state = ARP_INVALID;
        }
        if (arp_table[i].timeout > min)//记录一个最接近超时的
        {
            min = arp_table[i].timeout;
            minindex = i;
        }
    }
    if (updateflag == 0)//没有无效的项
    {
                strcpy(arp_table[minindex].ip,ip);
                strcpy(arp_table[minindex].mac,mac);
                arp_table[minindex].state = ARP_VALID;
                arp_table[minindex].timeout = now+ARP_TIMEOUT_SEC;
                updateflag = 1;
    }
}

/**
 * @brief 从arp表中根据ip地址查找mac地址
 * 
 * @param ip 欲转换的ip地址
 * @return uint8_t* mac地址，未找到时为NULL
 */
static uint8_t *arp_lookup(uint8_t *ip)
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        if (arp_table[i].state == ARP_VALID && memcmp(arp_table[i].ip, ip, NET_IP_LEN) == 0)
            return arp_table[i].mac;
    return NULL;
}

/**
 * @brief 发送一个arp请求
 *        你需要调用buf_init对txbuf进行初始化
 *        填写ARP报头，将ARP的opcode设置为ARP_REQUEST，注意大小端转换
 *        将ARP数据报发送到ethernet层
 * 
 * @param target_ip 想要知道的目标的ip地址
 */
static void arp_req(uint8_t *target_ip)
{
    // TODO
    buf_init(&txbuf, sizeof(arp_pkt_t));
    arp_pkt_t* arp = (arp_pkt_t *)txbuf.data;
    *arp = arp_init_pkt;
    strcpy(arp->target_ip,target_ip);
    arp->opcode = swap16(ARP_REQUEST);
    ethernet_out(&txbuf,ether_broadcast_mac,NET_PROTOCOL_ARP);
}

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，查看报文是否完整，
 *        检查项包括：硬件类型，协议类型，硬件地址长度，协议地址长度，操作类型
 *        
 *        接着，调用arp_update更新ARP表项
 *        查看arp_buf是否有效，如果有效，则说明ARP分组队列里面有待发送的数据包。
 *        即上一次调用arp_out()发送来自IP层的数据包时，由于没有找到对应的MAC地址进而先发送的ARP request报文
 *        此时，收到了该request的应答报文。然后，根据IP地址来查找ARM表项，如果能找到该IP地址对应的MAC地址，
 *        则将缓存的数据包arp_buf再发送到ethernet层。
 * 
 *        如果arp_buf无效，还需要判断接收到的报文是否为request请求报文，并且，该请求报文的目的IP正好是本机的IP地址，
 *        则认为是请求本机MAC地址的ARP请求报文，则回应一个响应报文（应答报文）。
 *        响应报文：需要调用buf_init初始化一个buf，填写ARP报头，目的IP和目的MAC需要填写为收到的ARP报的源IP和源MAC。
 * 
 * @param buf 要处理的数据包
 */
void arp_in(buf_t *buf)
{
    // TODO
    uint8_t *mac;
    arp_pkt_t * arp = (arp_pkt_t*)buf->data;
    int opcode = swap16(arp->opcode);
    if (arp->hw_type!=swap16(ARP_HW_ETHER)
        ||arp->pro_type!=swap16(NET_PROTOCOL_IP)
        ||arp->hw_len!=NET_MAC_LEN
        ||arp->pro_len!=NET_IP_LEN
        ||(arp->opcode!=swap16(ARP_REQUEST)&&arp->opcode!=swap16(ARP_REPLY)))
    {
        return;
    }
    arp_update(arp->sender_ip,arp->sender_mac,ARP_VALID);
    if (arp_buf.valid)//有待发的数据包
    {
        if((mac=arp_lookup(arp_buf.ip))!=NULL)
        {
            arp_buf.valid = 0;
            ethernet_out(&arp_buf.buf,mac,arp_buf.protocol);
        }
    }
    else
    {
        if ((arp->opcode==swap16(ARP_REQUEST))&&!(memcmp(arp->target_ip,net_if_ip,sizeof(uint8_t)*4)))
        {
            buf_init(&txbuf, sizeof(arp_pkt_t));
            arp_pkt_t* arpreply = (arp_pkt_t *)txbuf.data;
            *arpreply = arp_init_pkt;
            memcpy(arpreply->target_mac,arp->sender_mac,sizeof(uint8_t)*NET_MAC_LEN);
            memcpy(arpreply->target_ip,arp->sender_ip,sizeof(uint8_t)*NET_MAC_LEN);
            arpreply->opcode = swap16(ARP_REPLY);
            ethernet_out(&txbuf,arp->sender_mac,NET_PROTOCOL_ARP);
        }
        
    }
    
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你需要根据IP地址来查找ARP表
 *        如果能找到该IP地址对应的MAC地址，则将数据报直接发送给ethernet层
 *        如果没有找到对应的MAC地址，则需要先发一个ARP request报文。
 *        注意，需要将来自IP层的数据包缓存到arp_buf中，等待arp_in()能收到ARP request报文的应答报文
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO
    arp_buf_t* arp = (arp_buf_t*)buf->data;
    uint8_t* mac = arp_lookup(ip);
    if(mac != NULL)//表中找到目的mac
    {
        ethernet_out(buf,mac,protocol);
    }
    else
    {        
        arp_buf.buf = *buf;
        strcpy(arp_buf.ip,ip);
        arp_buf.protocol = protocol;
        arp_buf.valid = 1;//1 is valid
        arp_req(ip);
    }   
}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init()
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_table[i].state = ARP_INVALID;
    arp_buf.valid = 0;
    arp_req(net_if_ip);
}