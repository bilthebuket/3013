#ifndef MAILBOX_H
#define MAILBOX_H

typedef struct msg
{
	int iSender;
	int type;
	int value1;
	int value2;
} msg;

void init_boxes(int num_boxes);

void SendMsg(int iTo, msg* pMsg);

void RecvMsg(int iFrom, msg* pMsg);

#endif
