#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <errno.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include "sys/types.h"
#include <netinet/ip.h>
#include <string.h>
#include <sqlite3.h>


int flag = 0;

struct tcphdr{
	u_int16_t src;
	u_int16_t dst;
	u_int32_t seq;
	u_int32_t ack;
	u_int8_t tcp_rec:4, hlen:4;
	u_int16_t win;
	u_int16_t checksum;
	u_int16_t u_ptr;
};

typedef struct _return{
	int id;
	int flag;
}return_val;

void iptable_set(){
	printf("\n[+] iptable setting\n\n");
	system("iptables -F");
	system("iptables -A OUTPUT -j NFQUEUE --queue-num 0");
	system("iptables -A INPUT -j NFQUEUE --queue-num 0");
	//printf(system("iptables -L"));
	printf("\n\n");
}

void iptable_restore(){
	system("iptables -F");
	//printf(system("iptables -L"));
	printf("\n[+] TERMINATE THE PROGRAM\n");
}

void dump(u_char *data, int len){
	for(int i=0;i<len;i++){
		if(i%16==0 && i!=0)
			printf("\n");
		printf("%02x ", data[i]);
	}
}

static int callback(void *data, int argc, char **argv, char **azColName){
   int i;
   fprintf(stderr, "%s ", (const char*)data);
   
   for(i = 0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   
   flag = 1;
   printf("\n");
   return 0;
}


int check(u_char *data, int len){
	//int target_len = strlen(target);
	u_int16_t packet_len;
	u_int32_t ip_len;
	u_int32_t tcp_len;
	struct ip *ip_hdr;
	struct tcphdr *tcp_hdr;
	char *method[] = {"GET", "POST", "HEAD", "PUT", "DELETE", "OPTIONS"};
	char *http_met;
	
	int rc;
	sqlite3 *db;
	char *sql, *str, *add;
	char *insert;
	char *tmp;
	const char* d = "[+] url to block detected!\n";
	char *zErrMsg = 0;

	ip_hdr = (struct ip*)data;
	
	if(ip_hdr->ip_p != 6)
		return flag;


	ip_len = ip_hdr->ip_hl * 4;
	packet_len = ntohs(ip_hdr->ip_len);
	/*printf("\n[+] packet info\n");
	printf("ip header length : %d\n", ip_len);
	printf("packet length : %d\n", packet_len);*/

	tcp_hdr = (struct tcphdr *)(data + ip_len);
	tcp_len = tcp_hdr->hlen * 4;
	//printf("tcp header length : %d\n", tcp_len);
	
	data += ip_len;
	data += tcp_len;
	for(int i=0;i<6;i++){
		if(strstr(data, method[i])){
			//printf("\n[+] method : %s\n", method[i]);
			http_met = method[i];
			break;
		}
		if(i == 5) return flag;
	}
	//for(int i=0;i<len-ip_len-tcp_len;i++){
		if(strstr(data, "Host: ")){
				printf("\n[+] method : %s\n", http_met);
				//printf("%s\n",strstr(data, "Host: "));
				char *target;
				int idx = (u_char*)strstr(data, "\n")-data;
				data += idx + 1;
				idx = (u_char*)strstr(data, "\n")-data;
				target = (char*)malloc(sizeof(char)*(strlen(data)-idx));
				memset(target, 0x00, sizeof(char)*(strlen(data)-idx));				
				strncpy(target, strstr(data, "Host: ")+strlen("Host: "), idx-strlen("Host: "));				
				//printf("[+] target : %s\n",target);

				//printf("length : %d\n",strlen(target));

				rc = sqlite3_open("sql_db.db", &db);
				if(rc){
					fprintf(stderr, "[-] Can't open database : %s\n", sqlite3_errmsg(db));
					return(0);		
				}
				
				sql = (char *)malloc(sizeof(char)*256);
				memset(sql, 0x00, sizeof(char)*256);
				//strcpy(sql, "select * from block_url;");
				strcpy(sql, "select * from block_url where url=\"");				
				strncat(sql, target, strlen(target)-1);				
				
				add = "\";";			
				strncat(sql, add, strlen(add));						
				printf("[+] %s\n", sql);
	
				flag = 0;
				rc = sqlite3_exec(db, sql, callback, (void*)d, &zErrMsg);
				if(rc){
					fprintf(stderr, "[-] Can't open database : %s\n", sqlite3_errmsg(db));
					return(0);		
				}

				free(target);
				free(sql);
				//flag = 1;
				sqlite3_close(db);
				//printf("FLAG = %d\n",flag);
				return flag;
		}
		else{
			flag = 0;
			return flag;
		}
	
	//return flag;
}

return_val print_pkt(struct nfq_data *tb){
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
	u_int32_t mark,ifi;
	int ret;
	u_char *data;
	return_val ret_val;

	ph = nfq_get_msg_packet_hdr(tb);
	if(ph){
		ret_val.id = ntohl(ph->packet_id);
		//printf("hw_protocol=0x%04x hook=%u id=%u ",ntohs(ph->hw_protocol), ph->hook, id);
	}

	hwph = nfq_get_packet_hw(tb);
	if(hwph){
		int i, hlen = ntohs(hwph->hw_addrlen);

		/*printf("hw_src_addr=");
		for(i=0;i<hlen-1;i++)
			printf("%02x:", hwph->hw_addr[i]);
		printf("%02x ",hwph->hw_addr[hlen-1]);*/
	}

	ret = nfq_get_payload(tb, &data);
	if(ret >= 0){
		//printf("payload length = %d\n", ret);
		ret_val.flag = check(data,ret);
		//printf("return FLAG = %d\n", ret_val.flag);
	}

	return ret_val;
}

static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data){
	return_val ret_val;
	ret_val = print_pkt(nfa);
	//printf("entering callback\n");
	if(ret_val.flag)
		return nfq_set_verdict(qh, ret_val.id, NF_DROP, 0, NULL);
	else
		return nfq_set_verdict(qh, ret_val.id, NF_ACCEPT, 0, NULL);
}

int main(int argc, char **argv){
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	struct nfnl_handle *nh;
	int fd;
	int rv;
	char buf[4096] __attribute__((aligned));

	iptable_set();
		
	printf("[+] opening library handle\n");
	h = nfq_open();
	if(!h){
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	printf("[+] unbinding existing nf_queue handler for AF_INET (if any)\n");
	if(nfq_unbind_pf(h, AF_INET) < 0){
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	printf("[+] binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if(nfq_bind_pf(h, AF_INET) < 0){
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	printf("[+] binding this socket to queue '0'\n");
	qh = nfq_create_queue(h, 0, &cb, NULL);
	if(!qh){
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}
	printf("[+] setting copy_packet mode\n");
	if(nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0){
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	fd = nfq_fd(h);

	while(1){
		if((rv = recv(fd, buf, sizeof(buf), 0)) >= 0){
			//printf("packet received\n");
			nfq_handle_packet(h, buf, rv);
			continue;
		}

		if(rv < 0 && errno == ENOBUFS){
			printf("losing packet!\n");
			continue;
		}
		perror("recv failed");
		break;
	}

	printf("[+] unbinding from queue 0 \n");
	nfq_destroy_queue(qh);

	printf("[+] unbinding from AF_INET\n");
	nfq_unbind_pf(h, AF_INET);

	printf("[+] closing library handle\n");
	nfq_close(h);

	iptable_restore();

	return 0;
}
