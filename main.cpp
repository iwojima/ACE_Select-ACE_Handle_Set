

#include "main.h"
#include <ace/OS_NS_sys_socket.h>
#include <ace/Thread_Manager.h>

void *pthread_function_one(void *pArg);
void *pthread_function_two(void *pArg);
int handle_data(ACE_SOCK_Stream stream);

ACE_Handle_Set handle_set;
std::map<std::string , ACE_SOCK_Stream> STRING_STREAM;//string - ACE_SOCK_Stream 名值对

//using namespace ACE;
//#define PORT "1357"
int main(int argc , char *argv[])
{
	ACE_SOCK_Acceptor acceptor;
	ACE_SOCK_Stream local_stream , remote_stream;
	ACE_INET_Addr local_inet_addr , remote_inet_addr;
        
        printf("port = %d\n",atoi(PORT));

	local_inet_addr.set(atoi(PORT));
	acceptor.open(local_inet_addr , 1); //1 recue
	handle_set.set_bit(acceptor.get_handle());
	
	local_stream.set_handle(acceptor.get_handle());
	STRING_STREAM["127.0.0.1"]=local_stream;
	
	//pthread_t tid;
	//pthread_create(&tid , NULL , pthread_function , NULL); // new thread to handle data
	ACE_Thread_Manager::instance()->spawn(pthread_function_one);
	ACE_Thread_Manager::instance()->spawn(pthread_function_two);

	int result = 0;
	
	while(1)
	{
		result = acceptor.accept(remote_stream , &remote_inet_addr);
		if (result < 0) 
		{
			continue;
		}
		//
		printf("remote handle = %d\n",(remote_stream.get_handle()));
		//
		handle_set.set_bit(remote_stream.get_handle());
		handle_set.clr_bit(acceptor.get_handle());

		const char *host = remote_inet_addr.get_host_addr();
		STRING_STREAM[host]=remote_stream;
		
	}
	
	//pthread_join(tid , NULL);
	ACE_Thread_Manager::instance()->wait();
	ACE_Thread_Manager::instance()->cancel_all();

	return 0;
}

void *pthread_function_two(void *pArg)
{
	std::cout<<"pthread_function_two - pid = "<<ACE_OS::thr_self()<<std::endl;
}

void *pthread_function_one(void *pArg)
{
	int result = 0 , width = 0;
	ACE_Handle_Set active_handle_set;

	std::map<std::string , ACE_SOCK_Stream>::iterator iter;
	
	struct timeval tv = {1, 0};
	ACE_Time_Value atv(tv);
	
	std::cout<<"pthread_function_two - pid = "<<ACE_OS::thr_self()<<std::endl;
	printf("max_set = %d\n",(int)handle_set.max_set());
	
	while ( 1 )
	{
		active_handle_set = handle_set;
		width = (int)active_handle_set.max_set() + 1; //max_set() Returns the number of the large bit.  UNIX下最大描述符+1

		if (ACE::select(width , &active_handle_set , 0 , 0 , &atv) < 1)  //ACE::select 返回活动的句柄数目
		{
			continue;
		}

		/*
		for(int i = 0; i < active_handle_set.num_set() ; i++)
		{
			printf("handle = %d , num_set = %d\n",i ,active_handle_set.num_set());
		}
		*/
		
		int set_size = active_handle_set.num_set();
		if (set_size > 0)
		{
			std::cout<<"set_size = "<<set_size<<std::endl;
		}
		
		
		#if 0  //普通的ACE_Handle_Set轮询
		for (iter = STRING_STREAM.begin() ; iter != STRING_STREAM.end() ; iter++)
		{
			if (active_handle_set.is_set((iter->second).get_handle()))
			{
				ACE_INET_Addr inet_addr;
				(iter->second).get_remote_addr(inet_addr);
				std::cout<<"<==== The Data from "<<inet_addr.get_host_addr()<<" ====>"<<std::endl;
				handle_data(iter->second);
				active_handle_set.clr_bit(iter->second.get_handle());
			}
		}
		#endif
		
		
		ACE_Handle_Set_Iterator iterator(active_handle_set);
		//ACE_Handle_Set_Iterator迭代，更加高级
		ACE_SOCK_Stream tmp;
		
		ACE_HANDLE handle = iterator();
		
		for ( ;handle != ACE_INVALID_HANDLE; handle = iterator()) 
		{
			//
			ACE_INET_Addr inet_addr;
			tmp.set_handle(handle);
			tmp.get_remote_addr(inet_addr);
			std::cout<<"<==== The Data from "<<inet_addr.get_host_addr()<<" ====>"<<std::endl;
			handle_data(tmp);
		}
		
		
	}
}


int handle_data(ACE_SOCK_Stream stream)
{
	printf("handle_data() - remote stream = %d\n",stream.get_handle());
	
	ACE_Message_Block *mblk = new ACE_Message_Block(8);
	ACE_CDR::mb_align(mblk);
	int rs = stream.recv_n(mblk->wr_ptr() , 8);
	if (rs != 8)
	{
		printf("*ERROR* rs = %d\n",rs);
		stream.close();
		return -1;
	}
	
	mblk->wr_ptr(8);
	
	ACE_InputCDR cdr(mblk);
	
	ACE_CDR::ULong length = 0;
	ACE_CDR::Boolean b;
	
	cdr >> ACE_InputCDR::to_boolean(b);
	
	cdr.reset_byte_order(b);
	cdr >> length;

	if (length > 0) 
	{
		ACE_Message_Block *data = new ACE_Message_Block(length+1);
		rs = stream.recv_n(data->wr_ptr() , length);
		if (rs != length)
		{
			printf("*ERROR* Fail to recv data from network\n");
			mblk->release();
			mblk = 0;
			return -1;
		}
		data->wr_ptr(length);
		
		printf("data = %s\n",data->rd_ptr());
		data->release();
		data = 0;
		//delete data;
	}
	
	mblk->release();
	mblk = 0;
	
	
	//delete mblk;
	
	//stream.close();
	
	return 0;
}

/*
while (1)
	{
		//int width = (int)active_handles_.max_set() + 1;
		//if(select(width , active_handles_.fdset() , 0 , 0 , 0))
		width = (int)handle_set.max_set();
		
		printf("1 num = %d\n",handle_set.num_set());
		
		result = select(width , handle_set, 0 , 0 , 0);
		handle_set.sync(handle_set.max_set());
		
		printf("2 num = %d\n",handle_set.num_set());
		ACE_HANDLE handle = acceptor.get_handle();
		do
		{
			if(handle_set.is_set(handle))
			{
				printf("*Success*\n");
			}
			else
			{
				printf("*Pass*\n");
			}
			handle++;
		}while(handle <= handle_set.max_set());
		
		result = acceptor.accept(remote_stream , &remote_addr);
		if(result != 0)
		{
			printf("*ERROR* Fail to execute accept !\n");
		}
		
		handle_set.set_bit(remote_stream.get_handle());
		
		const char *hostaddr = remote_addr.get_host_addr();
		printf("hostaddr = %s\n",hostaddr);

		handle_set.clr_bit(acceptor.get_handle());
		handle_set.set_bit(remote_stream.get_handle());

		printf("<--------------------この人--------------------->\n");
	}
*/
