#define STATUS_BUFFER_SIZE 30
#define MAX_BACKGROUND_PROCESSES 5000

int buffer_count = 0, background_size = 0;


typedef struct {
    pid_t pid;
    int status;
} ProcessStatus;

ProcessStatus status_buffer[STATUS_BUFFER_SIZE];
pid_t background_pids[MAX_BACKGROUND_PROCESSES];


bool is_background_process(pid_t pid)
{
    for (int i = 0; i < background_size; i++)
    {
        if (background_pids[i] == pid)
        {
            return true;
        }
    }
    return false;
}


bool is_pipeline_in_background(pipeline *pipe_data)
{
    if (!pipe_data)
    {
        return false;
    }

    return (pipe_data->flags & INBACKGROUND) != 0;
}


void print_background_processes()
{
    fprintf(stdout, "Background processes (%d): ", background_size);
    for (int i = 0; i < background_size; i++)
    {
        fprintf(stdout, "%d ", background_pids[i]);
    }
    fprintf(stdout, "\n");
}


bool add_background_process(pid_t pid)
{
    if (background_size >= MAX_BACKGROUND_PROCESSES)
    {
        fprintf(stderr, "Background overflow error \n");
        return false;
    }
    background_pids[background_size++] = pid;
    return true;
}

bool remove_background_process(pid_t pid)
{
    for (int i = 0; i < background_size; i++)
    {
        if (background_pids[i] == pid)
        {
            background_pids[i] = background_pids[--background_size];
            return true;
        }
    }
    return false;
}
