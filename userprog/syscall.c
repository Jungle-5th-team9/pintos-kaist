#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

// #include "lib/user/syscall.h" /* makefile 에 이 경로가 포함이 안되어있어서 안되는 듯... makefile까지 건드는건 오바인거 같아서 이 방법은 폐기! */
// 위 경로의 syscall은 유저와 커널 간 인터페이스! 따라서 따로 구현해야함!!!
// syscall_handler가 실행하는거다

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

void check_address(void *addr);
void get_argument(void *rsp, int *argv, int argc);

/* Projects 2 and later. */

void halt(void);
void exit(int status);
// pid_t fork (const char *thread_name);
// int exec (const char *cmd_line);
// int wait (pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	// printf("system call!\n");

	// Getting the system call number from the interrupt frame /* %rax 는 시스템 콜 번호 */
	int syscall_number = f->R.rax;

	switch (syscall_number)
	{
	case SYS_HALT: /* Halt the operating system. */
		halt();
	case SYS_EXIT: /* Terminate this process. */
		exit((int)f->R.rdi);
		break;
		// case SYS_FORK: /* Clone current process. */
		// 	fork();
		// case SYS_EXEC: /* Switch current process. */
		// 	exec();
		// case SYS_WAIT: /* Wait for a child process to die. */
		// 	wait();
	case SYS_CREATE: /* Create a file with the given name and initial size. */
		f->R.rax = create((const char *)f->R.rdi, (unsigned)f->R.rsi);
		break;
	case SYS_REMOVE: /* Remove the file with the given name. */
		f->R.rax = remove((const char *)f->R.rdi);
		break;
	case SYS_OPEN: /* Open the file with the given name. */
		f->R.rax = open((const char *)f->R.rdi);
		break;
	case SYS_FILESIZE: /* Get the size of the open file. */
		f->R.rax = filesize((int)f->R.rdi);
		break;
	case SYS_READ: /* Read from an open file. */
		f->R.rax = read((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
		break;
	case SYS_WRITE: /* Write to an open file. */
		f->R.rax = write((int)f->R.rdi, (const void *)f->R.rsi, (unsigned)f->R.rdx);
		break;
	case SYS_SEEK: /* Change the next byte to be read or written in an open file. */
		seek((int)f->R.rdi, (unsigned)f->R.rsi);
		break;
	case SYS_TELL: /* Get the position of the next byte to be read or written in an open file. */
		f->R.rax = tell((int)f->R.rdi);
		break;
	case SYS_CLOSE: /* Close an open file. */
		close((int)f->R.rdi);
		break;
	default:
		// 지원되지 않는 시스템 콜 처리
		printf("Unknown system call: %d\n", syscall_number);
		thread_exit();
	}
}

/* Check if the address is in user space */
void check_address(void *addr)
{
	/* 포인터가 가리키는 주소가 유저영역의 주소인지 확인 */
	/* 잘못된 접근일 경우 프로세스 종료 */
	if (addr == NULL || !is_user_vaddr(addr))
	{
		exit(-1);
	}
}

/* Retrieve arguments from the user stack and store them in argv */
void get_argument(void *rsp, int *argv, int argc)
{
	for (int i = 0; i < argc; i++)
	{
		void *arg_ptr = (void *)((uint8_t *)rsp + i * sizeof(uint64_t));
		check_address(arg_ptr);
		argv[i] = *(int *)arg_ptr;
	}
}

/**
 * This function calls power_off() to shut down Pintos.
 * It should be used sparingly, as it might result in losing important information
 * such as deadlock situations.
 */

/**
 * @brief Halts the operating system.
 */
void halt(void)
{
	// power_off()를 호출해서 Pintos를 종료
	power_off(); // 이 함수는 웬만하면 사용되지 않아야 합니다. deadlock 상황에 대한 정보 등등 뭔가 조금 잃어 버릴지도 모릅니다.
}

/**
 * This function retrieves the currently running thread's structure,
 * prints the process termination message, and terminates the thread.
 */

/**
 * @brief Exits the current process.
 *
 * @param status The exit status of the process.
 */
void exit(int status)
{
	/* 실행중인 스레드 구조체를 가져옴 */
	/* 프로세스 종료 메시지 출력,
	출력 양식: “프로세스이름: exit(종료상태)” */
	/* 스레드 종료 */
	struct thread *cur = thread_current();
	printf("%s: exit(%d)\n", cur->name, status);
	thread_exit();
}

/**
 * This function creates a new file with the specified name and initial size.
 * It checks the validity of the file name address before creating the file.
 */

/**
 * @brief Creates a new file.
 *
 * @param file The name of the file to create.
 * @param initial_size The initial size of the file.
 * @return True if the file was successfully created, false otherwise.
 */
bool create(const char *file, unsigned initial_size)
{
	/* 파일 이름과 크기에 해당하는 파일 생성 */
	/* 파일 생성 성공 시 true 반환, 실패 시 false 반환 */
	check_address((void *)file);
	// 실제 파일 시스템 호출로 변경 필요
	return filesys_create(file, initial_size);
}

/**
 * This function removes the file with the specified name.
 * It checks the validity of the file name address before removing the file.
 */

/**
 * @brief Removes a file.
 *
 * @param file The name of the file to remove.
 * @return True if the file was successfully removed, false otherwise.
 */
bool remove(const char *file)
{
	/* 파일 이름에 해당하는 파일을 제거 */
	/* 파일 제거 성공 시 true 반환, 실패 시 false 반환 */
	check_address((void *)file);
	// 실제 파일 시스템 호출로 변경 필요
	return filesys_remove(file);
}

/**
 * @brief Opens a file and returns a file descriptor.
 *
 * @param file The name of the file to open.
 * @return The file descriptor if successful, -1 otherwise.
 */
int open(const char *file)
{
	check_address((void *)file);		 // 주어진 파일 이름 주소가 유효한지 확인합니다.
	struct file *f = filesys_open(file); // 파일 시스템에서 파일을 엽니다.
	if (!f)
	{
		return -1; // 파일을 열 수 없는 경우 -1을 반환합니다.
	}
	int fd = thread_current()->next_fd++; // 다음 파일 디스크립터를 가져오고 증가시킵니다.
	thread_current()->fd_table[fd] = f;	  // 파일 디스크립터 테이블에 파일 포인터를 저장합니다.
	return fd;							  // 파일 디스크립터를 반환합니다.
}

/**
 * @brief Returns the size of the file in bytes.
 *
 * @param fd The file descriptor of the file.
 * @return The size of the file in bytes if successful, -1 otherwise.
 */
int filesize(int fd)
{
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (!f)
	{
		return -1; // 파일이 열려 있지 않은 경우 -1을 반환합니다.
	}
	return file_length(f); // 파일의 길이를 반환합니다.
}

/**
 * @brief Reads data from a file into a buffer.
 *
 * @param fd The file descriptor of the file.
 * @param buffer The buffer to store the data.
 * @param size The number of bytes to read.
 * @return The number of bytes read if successful, -1 otherwise.
 */
int read(int fd, void *buffer, unsigned size)
{
	check_address(buffer); // 주어진 버퍼 주소가 유효한지 확인합니다.
	if (fd == STDIN_FILENO)
	{
		// stdin에서 읽는 경우 처리
		return -1; // 표준 입력에서 읽는 경우는 현재 지원하지 않으므로 -1을 반환합니다.
	}
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (!f)
	{
		return -1; // 파일이 열려 있지 않은 경우 -1을 반환합니다.
	}
	return file_read(f, buffer, size); // 파일에서 데이터를 읽고, 읽은 바이트 수를 반환합니다.
}

/**
 * @brief Writes data from a buffer to a file.
 *
 * @param fd The file descriptor of the file.
 * @param buffer The buffer containing the data.
 * @param size The number of bytes to write.
 * @return The number of bytes written if successful, -1 otherwise.
 */
int write(int fd, const void *buffer, unsigned size)
{
	check_address((void *)buffer); // 주어진 버퍼 주소가 유효한지 확인합니다.
	if (fd == STDOUT_FILENO)
	{
		putbuf(buffer, size); // 표준 출력에 데이터를 씁니다.
		return size;		  // 쓴 바이트 수를 반환합니다.
	}
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (!f)
	{
		return -1; // 파일이 열려 있지 않은 경우 -1을 반환합니다.
	}
	return file_write(f, buffer, size); // 파일에 데이터를 쓰고, 쓴 바이트 수를 반환합니다.
}

/**
 * @brief Sets the file position to a given value.
 *
 * @param fd The file descriptor of the file.
 * @param position The new position in the file.
 */
void seek(int fd, unsigned position)
{
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (f)
	{
		file_seek(f, position); // 파일의 위치를 지정한 위치로 이동합니다.
	}
}

/**
 * @brief Returns the current position in the file.
 *
 * @param fd The file descriptor of the file.
 * @return The current position in the file if successful, -1 otherwise.
 */
unsigned tell(int fd)
{
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (f)
	{
		return file_tell(f); // 파일의 현재 위치를 반환합니다.
	}
	return -1; // 파일이 열려 있지 않은 경우 -1을 반환합니다.
}

/**
 * @brief Closes the file.
 *
 * @param fd The file descriptor of the file.
 */
void close(int fd)
{
	struct file *f = thread_current()->fd_table[fd]; // 파일 디스크립터 테이블에서 파일 포인터를 가져옵니다.
	if (f)
	{
		file_close(f);						   // 파일을 닫습니다.
		thread_current()->fd_table[fd] = NULL; // 파일 디스크립터 테이블에서 파일 포인터를 제거합니다.
	}
}
