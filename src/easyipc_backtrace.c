#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <ucontext.h>
#include "easyipc_base.h"
#include "easyipc_daemon.h"

#ifdef ENABLE_IPC_SIGNAL


#ifdef __arm__

#if USE_BACKTRACE
void DebugBacktrace(unsigned int sn , siginfo_t  *si , void *ptr);

typedef struct
{
	const char *dli_fname;	/* File name of defining object.  */
	void *dli_fbase;		/* Load address of that object.  */
	const char *dli_sname;	/* Name of nearest symbol.比如函数名*/
	void *dli_saddr;		/* Exact value of nearest symbol.比如函数的起始地址*/
} Dl_info;

struct ucontext_ce123 {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	struct sigcontext uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
}ucontext_ce123_;

struct sigframe_ce123 {  
    struct sigcontext sc;//保存一组寄存器上下文  
    unsigned long extramask[1];  
    unsigned long retcode;//保存返回地址  
    //struct aux_sigframe aux __attribute__((aligned(8)));  
}sigframe_ce123; 

int backtrace_ce123 (void **array, int size);
char ** backtrace_symbols_ce123 (void *const *array, int size);


int backtrace_ce123 (void **array, int size)
{
	if (size <= 0)
		return 0;

	int *fp = 0, *next_fp = 0;
	int cnt = 0;
	int ret = 0;

	__asm__(
		"mov %0, fp\n" 
		: "=r"(fp)
	);


	array[cnt++] = (void *)(*(fp-1));

	next_fp = (int *)(*(fp-3));
	while((cnt <= size) && (next_fp != 0))
	{
		array[cnt++] = (void *)*(next_fp - 1);
		next_fp = (int *)(*(next_fp-3));
	}


	ret = ((cnt <= size)?cnt:size);
	printf("Backstrace (%d deep)\n", ret);

	return ret;
}

char ** backtrace_symbols_ce123 (void *const *array, int size)
{
# define WORD_WIDTH 8
	Dl_info info[size];
	int status[size];
	int cnt;
	size_t total = 0;
	char **result;

	/* Fill in the information we can get from `dladdr'.  */
	for (cnt = 0; cnt < size; ++cnt)
	{
		status[cnt] = dladdr(array[cnt], &info[cnt]);
		if (status[cnt] && info[cnt].dli_fname && info[cnt].dli_fname[0] != '\0')
		/* We have some info, compute the length of the string which will be
		"<file-name>(<sym-name>) [+offset].  */
		total += (strlen (info[cnt].dli_fname ?: "")
			+ (info[cnt].dli_sname ? strlen (info[cnt].dli_sname) + 3 + WORD_WIDTH + 3 : 1)
			+ WORD_WIDTH + 5);
		else
			total += 5 + WORD_WIDTH;
	}


	/* Allocate memory for the result.  */
	result = (char **) malloc (size * sizeof (char *) + total);
	if (result != NULL)
	{
		char *last = (char *) (result + size);

		for (cnt = 0; cnt < size; ++cnt)
		{
			result[cnt] = last;

			if (status[cnt] && info[cnt].dli_fname && info[cnt].dli_fname[0] != '\0')
			{
				char buf[20];

				if (array[cnt] >= (void *) info[cnt].dli_saddr)
					sprintf (buf, "+%#lx", \
						(unsigned long)(array[cnt] - info[cnt].dli_saddr));
				else
					sprintf (buf, "-%#lx", \
						(unsigned long)(info[cnt].dli_saddr - array[cnt]));

				last += 1 + sprintf (last, "%s%s%s%s%s[%p]",
					info[cnt].dli_fname ?: "",
					info[cnt].dli_sname ? "(" : "",
					info[cnt].dli_sname ?: "",
					info[cnt].dli_sname ? buf : "",
					info[cnt].dli_sname ? ") " : " ",
					array[cnt]);
			}
			else
				last += 1 + sprintf (last, "[%p]", array[cnt]);
		}
		assert (last <= (char *) result + size * sizeof (char *) + total);
	}

	return result;
}

#endif
#endif
/* SIGSEGV信号的处理函数，回溯栈，打印函数的调用关系*/
void DebugBacktrace(unsigned int sn , siginfo_t  *si , void *ptr)
{
	#ifdef __arm__	
	#if USE_BACKTRACE
	char upload_log[IPC_CMDLINE_MAX_SIZE-128]={0};
	size_t save_size=0;

	if(NULL != ptr)
	{
		//printf("\n\nunhandled page fault (%d) at: 0x%08x\n", si->si_signo,si->si_addr);

		struct ucontext_ce123 *ucontext = (struct ucontext_ce123 *)ptr;

		int pc = ucontext->uc_mcontext.arm_pc;		

		void *pc_array[1]; 
		pc_array[0] = pc;
		char **pc_name = backtrace_symbols_ce123(pc_array, 1);
		//printf("%d: %s\n", 0, *pc_name);

#define SIZE 100
		void *array[SIZE];
		int size, i;
		char **strings;
		size = backtrace_ce123(array, SIZE);
		strings = backtrace_symbols_ce123(array, size);	
		printf("size=%d\r\n",size);
		for(i=0;i<size;i++)
		{
			printf("%02d:[%s]\n",i,strings[i]);
			snprintf(upload_log+save_size,IPC_CMDLINE_MAX_SIZE-128-save_size,"%02d:[%s]\n",i,strings[i]);
			save_size += (strlen(strings[i]))+5;
		}
		free(strings);
		strings=NULL;
		usleep(100*1000);
		ipc_log(ipc_get_default_handle(),ENUM_IPC_LOG_LEVEL_ERR,"%s",upload_log);
	}
	#endif
	#endif
	ipc_hangup_record(sn);
	exit(0);
}


void ipc_backtrace_init()
{
	struct sigaction s;
	s.sa_flags = SA_SIGINFO;
	s.sa_sigaction = (void *)DebugBacktrace;
	sigaction (SIGSEGV,&s,NULL);  
	sigaction (SIGINT,&s,NULL);  
	sigaction (SIGQUIT,&s,NULL);  
	sigaction (SIGILL,&s,NULL);  
	sigaction (SIGTRAP,&s,NULL);  
	sigaction (SIGABRT,&s,NULL);  
	sigaction (SIGBUS,&s,NULL);  
	sigaction (SIGFPE,&s,NULL);  
	sigaction (SIGTERM,&s,NULL);  
	sigaction (SIGXCPU,&s,NULL);  
	sigaction (SIGXFSZ,&s,NULL);  
	sigaction (SIGPWR,&s,NULL);
	sigaction (SIGTSTP,&s,NULL);
}

#endif
