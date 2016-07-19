//
//  QNGetAddrInfo.c
//  HappyDNS
//
//  Created by bailong on 16/7/19.
//  Copyright © 2016年 Qiniu Cloud Storage. All rights reserved.
//

#include "string.h"
#include "netdb.h"
#include "stdlib.h"

#include "qn_getaddrinfo.h"

//fast judge domain or ip, not verify ip right.
static int isIp(const char* domain) {
    size_t l = strlen(domain);
    if (l > 15 || l < 7) {
        return 0;
    }

    for (const char* p = domain; p < domain + l; p++) {
        if ((*p < '0' || *p > '9') && *p != '.') {
            return 0;
        }
    }
    return 1;
}

static struct addrinfo* addrinfo_clone(struct addrinfo* addr) {
    struct addrinfo* ai;
    ai = calloc(sizeof(struct addrinfo) + addr->ai_addrlen, 1);
    if (ai) {
        memcpy(ai, addr, sizeof(struct addrinfo));
        ai->ai_addr = (void*)(ai + 1);
        memcpy(ai->ai_addr, addr->ai_addr, addr->ai_addrlen);
        if (addr->ai_canonname) {
            ai->ai_canonname = strdup(addr->ai_canonname);
        }
        ai->ai_next = NULL;
    }
    return ai;
}

static void append_addrinfo(struct addrinfo** head, struct addrinfo* addr) {
    if (*head == NULL) {
        *head = addr;
        return;
    }
    struct addrinfo* ai = *head;
    while (ai->ai_next != NULL) {
        ai = ai->ai_next;
    }
    ai->ai_next = addr;
}

void qn_free_ips_ret(qn_ips_ret* ip_list) {
    if (ip_list == NULL) {
        return;
    }
    char** p = ip_list->ips;
    while (*p != NULL) {
        free(*p);
        p++;
    }
    free(ip_list);
}

static qn_dns_callback dns_callback = NULL;
int qn_getaddrinfo(const char* hostname, const char* servname, const struct addrinfo* hints, struct addrinfo** res) {
    if (dns_callback == NULL || hostname == NULL || isIp(hostname)) {
        return getaddrinfo(hostname, servname, hints, res);
    }

    qn_ips_ret* ret = dns_callback(hostname);
    if (ret == NULL) {
        return EAI_NODATA;
    }
    if (ret->ips[0] == NULL) {
        qn_free_ips_ret(ret);
        return EAI_NODATA;
    }
    int i;
    struct addrinfo* ai = NULL;
    struct addrinfo* store = NULL;
    int r = 0;
    for (i = 0; ret->ips[i] != NULL; i++) {
        r = getaddrinfo(ret->ips[i], servname, hints, &ai);
        if (r != 0) {
            break;
        }
        struct addrinfo* temp = ai;
        ai = addrinfo_clone(ai);
        append_addrinfo(&store, ai);
        freeaddrinfo(temp);
        ai = NULL;
    }
    qn_free_ips_ret(ret);
    if (r != 0) {
        qn_freeaddrinfo(store);
        return r;
    }
    *res = store;
    return 0;
}

void qn_freeaddrinfo(struct addrinfo* ai) {
    if (ai == NULL) {
        return;
    }
    struct addrinfo* next;
    do {
        next = ai->ai_next;
        if (ai->ai_canonname)
            free(ai->ai_canonname);
        /* no need to free(ai->ai_addr) */
        free(ai);
        ai = next;
    } while (ai);
}

void qn_set_dns_callback(qn_dns_callback cb) {
    dns_callback = cb;
}