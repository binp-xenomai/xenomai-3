/**
 * This file is part of the Xenomai project.
 *
 * @note Copyright (C) 2009 Philippe Gerum <rpm@xenomai.org> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <rtdm/rtipc.h>
#include "internal.h"

static struct rtipc_protocol *protocols[IPCPROTO_MAX] = {
#ifdef CONFIG_XENO_DRIVERS_RTIPC_XDDP
	[IPCPROTO_XDDP - 1] = &xddp_proto_driver,
#endif
#ifdef CONFIG_XENO_DRIVERS_RTIPC_IDDP
	[IPCPROTO_IDDP - 1] = &iddp_proto_driver,
#endif
};

int rtipc_get_arg(rtdm_user_info_t *user_info,
		  void *dst, const void *src, size_t len)
{
	if (user_info) {
		if (rtdm_safe_copy_from_user(user_info, dst, src, len))
			return -EFAULT;
	} else
		memcpy(dst, src, len);

	return 0;
}

int rtipc_put_arg(rtdm_user_info_t *user_info,
		  void *dst, const void *src, size_t len)
{
	if (user_info) {
		if (rtdm_safe_copy_to_user(user_info, dst, src, len))
			return -EFAULT;
	} else
		memcpy(dst, src, len);

	return 0;
}

static int rtipc_socket(struct rtdm_dev_context *context,
			rtdm_user_info_t *user_info, int protocol)
{
	struct rtipc_protocol *proto;
	struct rtipc_private *p;

	if (protocol < 0 || protocol >= IPCPROTO_MAX)
		return -EPROTONOSUPPORT;

	if (protocol == IPCPROTO_IPC)
		/* Default protocol is IDDP */
		protocol = IPCPROTO_IDDP;

	proto = protocols[protocol - 1];
	if (proto == NULL)	/* Not compiled in? */
		return -ENOPROTOOPT;

	p = rtdm_context_to_private(context);
	p->proto = proto;
	p->state = xnmalloc(proto->proto_statesz);
	if (p->state == NULL)
		return -ENOMEM;

	return proto->proto_ops.socket(p, user_info);
}

static int rtipc_close(struct rtdm_dev_context *context,
		       rtdm_user_info_t *user_info)
{
	struct rtipc_private *p;
	int ret;

	p = rtdm_context_to_private(context);
	ret = p->proto->proto_ops.close(p, user_info);
	if (ret)
		return ret;

	xnfree(p->state);

	return 0;
}

static ssize_t rtipc_recvmsg(struct rtdm_dev_context *context,
			     rtdm_user_info_t *user_info,
			     struct msghdr *msg, int flags)
{
	struct rtipc_private *p = rtdm_context_to_private(context);
	return p->proto->proto_ops.recvmsg(p, user_info, msg, flags);
}

static ssize_t rtipc_sendmsg(struct rtdm_dev_context *context,
			     rtdm_user_info_t *user_info,
			     const struct msghdr *msg, int flags)
{
	struct rtipc_private *p = rtdm_context_to_private(context);
	return p->proto->proto_ops.sendmsg(p, user_info, msg, flags);
}

static ssize_t rtipc_read(struct rtdm_dev_context *context,
			  rtdm_user_info_t *user_info,
			  void *buf, size_t len)
{
	struct rtipc_private *p = rtdm_context_to_private(context);
	return p->proto->proto_ops.read(p, user_info, buf, len);
}

static ssize_t rtipc_write(struct rtdm_dev_context *context,
			   rtdm_user_info_t *user_info,
			   const void *buf, size_t len)
{
	struct rtipc_private *p = rtdm_context_to_private(context);
	return p->proto->proto_ops.write(p, user_info, buf, len);
}

static int rtipc_ioctl(struct rtdm_dev_context *context,
		       rtdm_user_info_t *user_info,
		       unsigned int request, void *arg)
{
	struct rtipc_private *p = rtdm_context_to_private(context);
	return p->proto->proto_ops.ioctl(p, user_info, request, arg);
}

static struct rtdm_device device = {
	.struct_version =	RTDM_DEVICE_STRUCT_VER,
	.device_flags	=	RTDM_PROTOCOL_DEVICE,
	.context_size	=	sizeof(struct rtipc_private),
	.device_name	=	"rtipc",
	.protocol_family=	PF_RTIPC,
	.socket_type	=	SOCK_DGRAM,
	.socket_rt	=	rtipc_socket,
	.socket_nrt	=	rtipc_socket,
	.ops = {
		.close_rt	=	rtipc_close,
		.close_nrt	=	rtipc_close,
		.recvmsg_rt	=	rtipc_recvmsg,
		.recvmsg_nrt	=	NULL,
		.sendmsg_rt	=	rtipc_sendmsg,
		.sendmsg_nrt	=	NULL,
		.ioctl_rt	=	rtipc_ioctl,
		.ioctl_nrt	=	rtipc_ioctl,
		.read_rt	=	rtipc_read,
		.read_nrt	=	NULL,
		.write_rt	=	rtipc_write,
		.write_nrt	=	NULL,
	},
	.device_class		=	RTDM_CLASS_RTIPC,
	.device_sub_class	=	RTDM_SUBCLASS_GENERIC,
	.profile_version	=	1,
	.driver_name		=	"rtipc",
	.driver_version		=	RTDM_DRIVER_VER(1, 0, 0),
	.peripheral_name	=	"Real-time IPC interface",
	.proc_name		=	device.device_name,
	.provider_name		=	"Philippe Gerum (xenomai.org)",
};

int __init __rtipc_init(void)
{
	int ret, n;

	for (n = 0; n < IPCPROTO_MAX; n++) {
		if (protocols[n] && protocols[n]->proto_init) {
			ret = protocols[n]->proto_init();
			if (ret)
				return ret;
		}
	}

	return rtdm_dev_register(&device);
}

void __rtipc_exit(void)
{
	rtdm_dev_unregister(&device, 1000);
}

module_init(__rtipc_init);
module_exit(__rtipc_exit);

MODULE_LICENSE("GPL");
