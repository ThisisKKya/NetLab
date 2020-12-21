#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include <string.h>

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，检查项包括：版本号、总长度、首部长度等。
 * 
 *        接着，计算头部校验和，注意：需要先把头部校验和字段缓存起来，再将校验和字段清零，
 *        调用checksum16()函数计算头部检验和，比较计算的结果与之前缓存的校验和是否一致，
 *        如果不一致，则不处理该数据报。
 * 
 *        检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报。
 * 
 *        检查IP报头的协议字段：
 *        如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
 *        如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
 *        如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
 *          
 * @param buf 要处理的包
 */
void ip_in(buf_t *buf)
{
    // TODO 
    ip_hdr_t* ip=(ip_hdr_t* )buf->data;
    if (ip->version!=IP_VERSION_4)
    {
        return;
    }
    if(checksum16((uint16_t*)ip,ip->hdr_len*IP_HDR_LEN_PER_BYTE)!=0)
        return;//校验失败
    if(!(memcmp(ip->dest_ip,net_if_ip,sizeof(uint8_t)*NET_IP_LEN)))//目的为本机ip
    {
        if(ip->protocol==NET_PROTOCOL_ICMP)
        {
            buf_remove_header(buf,sizeof(ip_hdr_t));
            icmp_in(buf,ip->src_ip);
        }
        else if (ip->protocol == NET_PROTOCOL_UDP )
        {
            buf_remove_header(buf,sizeof(udp_hdr_t));
            udp_in(buf,ip->src_ip);
        }
        else
        {
            icmp_unreachable(buf,ip->src_ip,ICMP_CODE_PROTOCOL_UNREACH);
        }
    }

    
}

/**
 * @brief 处理一个要发送的ip分片
 *        你需要调用buf_add_header增加IP数据报头部缓存空间。
 *        填写IP数据报头部字段。
 *        将checksum字段填0，再调用checksum16()函数计算校验和，并将计算后的结果填写到checksum字段中。
 *        将封装后的IP数据报发送到arp层。
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TODO
    buf_add_header(buf,sizeof(ip_hdr_t));
    ip_hdr_t* ipf =(ip_hdr_t*)buf->data;
    ipf->hdr_len = 5;
    ipf->version = IP_VERSION_4;
    ipf->tos=0;
    ipf->total_len=swap16(buf->len);
    ipf->id=swap16(id);
    switch (mf)
    {
    case 0:
        ipf->flags_fragment=swap16(offset);
        break;
    case 1:
        ipf->flags_fragment=swap16(offset)|IP_MORE_FRAGMENT;
        break;
    }
    ipf->ttl=IP_DEFALUT_TTL;
    ipf->protocol=protocol;
    ipf->hdr_checksum=0x0000;
    memcpy(ipf->src_ip,net_if_ip,sizeof(uint8_t)*NET_IP_LEN);
    memcpy(ipf->dest_ip,ip,sizeof(uint8_t)*NET_IP_LEN);
    ipf->hdr_checksum=checksum16((uint16_t*)ipf,ipf->hdr_len*IP_HDR_LEN_PER_BYTE);
    arp_out(buf,ip,NET_PROTOCOL_IP);
}

/**
 * @brief 处理一个要发送的ip数据包
 *        你首先需要检查需要发送的IP数据报是否大于以太网帧的最大包长（1500字节 - ip报头长度）。
 *        
 *        如果超过，则需要分片发送。 
 *        分片步骤：
 *        （1）调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - ip报头长度）
 *        （2）将数据报截断，每个截断后的包长度 = 以太网帧的最大包长，调用ip_fragment_out()函数发送出去
 *        （3）如果截断后最后的一个分片小于或等于以太网帧的最大包长，
 *             调用buf_init()函数初始化buf，长度为该分片大小，再调用ip_fragment_out()函数发送出去
 *             注意：id为IP数据报的分片标识，从0开始编号，每增加一个分片，自加1。最后一个分片的MF = 0
 *    
 *        如果没有超过以太网帧的最大包长，则直接调用调用ip_fragment_out()函数发送出去。
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO 
    ip_hdr_t* ipo = (ip_hdr_t*)buf->data;
    int maxlen=ETHERNET_MTU - sizeof(ip_hdr_t);
    int offset_index =0;
    int mf_0 = 0;
    int mf_1 = 1;
    static int id = 0;
    int offset = 0;
    while(buf->len> maxlen)//需要分片
    {
        /* code */
        buf_init(&txbuf,maxlen);
        memcpy(txbuf.data,buf->data,maxlen);
        ip_fragment_out(&txbuf,ip,protocol,id,offset,mf_1);
        offset += maxlen/IP_HDR_OFFSET_PER_BYTE;
        buf->len -= maxlen;
        buf->data += maxlen;
    }
    ip_fragment_out(buf,ip,protocol,id,offset,mf_0);
    id++;
}
