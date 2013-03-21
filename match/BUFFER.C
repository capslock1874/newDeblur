
#include "mplex.h"
/******************************************************************
	Buffer_Clean
	���DTSС��ʵ�ʵ�SCR����ú�����FIFO������������ݣ����������Щ
	�����Ѿ���������ˡ�

******************************************************************/

void buffer_clean (buffer, SCR)
Buffer_struc  *buffer;
Timecode_struc *SCR;
{
    Buffer_queue *pointer;

    while ((buffer->first != NULL) &&
	(comp_timecode(&buffer->first->DTS, SCR)))
    {
	pointer = buffer->first;
	buffer->first = buffer->first->next;
	free (pointer);	
    }
}

/******************************************************************
	Buffer_Space
	���ػ����п��õĿռ��С��
******************************************************************/

unsigned  int buffer_space (buffer)
Buffer_struc *buffer;
{
    unsigned int used_bytes;
    Buffer_queue *pointer;

    pointer=buffer->first;
    used_bytes=0;

    while (pointer != NULL)
    {
	used_bytes += pointer->size;
	pointer = pointer->next;
    }

    return (buffer->max_size - used_bytes);

}

/******************************************************************
	Queue_Buffer
	��FIFO�����м�������

******************************************************************/

void queue_buffer (buffer, bytes, TS)
Buffer_struc *buffer;
unsigned int bytes;
Timecode_struc *TS;
{
    Buffer_queue *pointer;

    pointer=buffer->first;
    if (pointer==NULL)
    {
	buffer->first = (Buffer_queue*) malloc (sizeof (Buffer_queue));
	buffer->first->size = bytes;
	buffer->first->next=NULL;
	copy_timecode (TS, &buffer->first->DTS);
    } else
    {
	while ((pointer->next)!=NULL)
	{
	    pointer = pointer->next;
	}
    
	pointer->next = (Buffer_queue*) malloc (sizeof (Buffer_queue));
	pointer->next->size = bytes;
	pointer->next->next = NULL;
	copy_timecode (TS, &pointer->next->DTS);
    }
}