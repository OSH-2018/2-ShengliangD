#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#include <stdlib.h>

#define ARG_NUM_MAX 256
#define JOB_NUM_MAX 256
typedef struct {
    char *args[ARG_NUM_MAX];

    int stdin;
    int stdout;
    int stderr;    
} job_cmd_t;
job_cmd_t job_cmds[JOB_NUM_MAX];

char *skip_space(char **ptr) {
    while (**ptr == ' ')
        ++*ptr;
    return *ptr;
}

// 复制到回车或 stop
char *get_string_until(char stop, char **ptr) {
    int cnt = 0;
    while ((*ptr)[cnt] != stop && (*ptr)[cnt] != '\n' && (*ptr)[cnt] != '\0')
        ++cnt;

    // 申请内存，复制
    char *ret = (char *)malloc(sizeof(char)*(cnt+1));
    for (int i = 0; i < cnt; ++i)
        ret[i] = (*ptr)[i];
    *ptr += cnt;

    return ret;
}

job_cmd_t parse_job_cmd(char **seek) {
    job_cmd_t jcmd;
    jcmd.stdin = jcmd.stdout = jcmd.stderr = -1;

    int cnt = 0;
    skip_space(seek);
    while (**seek != '\0' && **seek != '|') {
        char *fname = NULL;
        int append = 0;
        switch (**seek) {
            case '\'':  // 到下一个 '\'' 为止
                ++*seek;
                jcmd.args[cnt++] = get_string_until('\'', seek);
                ++*seek;
                break;
            case '\"':  // 到下一个 '"' 为止
                ++*seek;
                jcmd.args[cnt++] = get_string_until('\"', seek);
                ++*seek;
                break;
            case '>':  // 检查下一个字符是 '>' 还是其他
                ++*seek;
                switch (**seek) {
                    case '>':  // 追加
                        ++*seek;
                        append = 1;
                    default:
                        append = 0;
                }
                skip_space(seek);
                fname = get_string_until(' ', seek);
                jcmd.stdout = open(fname, O_CREAT | O_WRONLY | (append? O_APPEND : 0));
                free(fname);
                if (jcmd.stdout == -1) {
                    perror("STDIN");
                }
                break;
            case '<':
                ++*seek;
                skip_space(seek);
                fname = get_string_until(' ', seek);
                jcmd.stdin = open(fname, O_RDONLY);
                free(fname);
                if (jcmd.stdin == -1) {
                    perror("STDOUT");
                }
                break;
            default:  // 当做普通连续字符串对待
                jcmd.args[cnt++] = get_string_until(' ', seek);
        }
        skip_space(seek);
    }
    jcmd.args[cnt] = NULL;
    return jcmd;
}

void parse_line(char **seek) {
    int cnt = 0;
    while (1) {
        job_cmds[cnt++] = parse_job_cmd(seek);
        skip_space(seek);
        if (**seek != '|')
            break;
        ++*seek;
    }
    job_cmds[cnt].args[0] = NULL;
}

void run_jobs() {
    job_cmd_t *jcmd = job_cmds;

    int prev_stdout = -1;
    while (jcmd->args[0] != NULL) {
        if (strcmp(jcmd->args[0], "cd") == 0) {
            if (jcmd->args[1])
                chdir(jcmd->args[1]);
            continue;
        }
        if (strcmp(jcmd->args[0], "pwd") == 0) {  // TODO
            char wd[4096];
            puts(getcwd(wd, 4096));
            continue;
        }
        if (strcmp(jcmd->args[0], "exit") == 0)  // TODO
            return;
        if (strcmp(jcmd->args[0], "export") == 0)  // TODO
            return;

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_stdout == 0)
            if (jcmd->stdin != -1)
                dup2(jcmd->stdin, 0);
            if (jcmd->stdout != -1)
                dup2(jcmd->stdout, 1);
            if (jcmd->stderr != -1)
                dup2(jcmd->stderr, 2);
            execvp(jcmd->args[0], jcmd->args);
            exit(255);
        }

        ++jcmd;
    }
}

int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    while (1) {
        /* 提示符 */
        printf("# ");
        fflush(stdin);
        fgets(cmd, 256, stdin);

        /* 清理结尾的换行符 */
        {
            int i;
            for (i = 0; cmd[i] != '\n'; i++)
                ;
            cmd[i] = '\0';
        }

        /* 拆解命令行
            以管道为单位切分，填充 job 数据结构，然后逐一执行
         */
        {
            char *seek = cmd;
            parse_line(&seek);
        }

        run_jobs();
        
        // 等待所有子进程结束
        while (wait(NULL) > 0)
            ;
    }
}
