#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>

#define ARG_NUM_MAX 256
#define JOB_NUM_MAX 256
typedef struct {
    char *args[ARG_NUM_MAX];

    char *stdin;
    
    char *stdout;
    int stdout_append;  // 是否追加
    
    char *stderr;
    int stderr_append;  // 是否追加
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
    jcmd.stdin = jcmd.stdout = jcmd.stderr = NULL;
    jcmd.stdout_append = jcmd.stderr_append = 0;

    int cnt = 0;
    skip_space(seek);
    while (**seek != '\0' && **seek != '|') {
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
                        jcmd.stdout_append = 1;
                    default:
                        jcmd.stdout_append = 0;
                }
                free(jcmd.stdout);
                jcmd.stdout = get_string_until(' ', seek);
                break;
            case '<':
                free(jcmd.stdin);
                jcmd.stdin = get_string_until(' ', seek);
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
    // fork，记录 job，连接管道（注意 pwd）
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
            execvp(jcmd->args[0], jcmd->args);
            exit(255);
        }

        ++jcmd;

        wait(NULL);
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
    }
}
