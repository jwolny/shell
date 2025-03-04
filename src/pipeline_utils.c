int countCommands(pipeline *p)
{
    int c;
    commandseq * commands= p->commands;

    if (commands==NULL){
        return 0;
    }
    c=0;
    do{
        ++c;
        commands= commands->next;
    }while (commands!=p->commands);
    return c;
}

int countPipelines(pipelineseq *seq)
{
    if (seq == NULL)
    {
        return 0;
    }

    int c = 0;
    pipelineseq *start = seq;
    do {
        ++c;
        seq = seq->next;
    } while (seq != start);

    return c;
}

bool empty_commands(pipeline *pipe)
{
    int num_commands = countCommands(pipe);
    if (num_commands <= 1)
    {
        return false;
    }
    command *com = pipe->commands->com;
    for (int i = 0; i < num_commands; i++)
    {
        if (com == NULL)
        {
            return true;
        }
        com = pipe->commands->next->com;
        pipe->commands = pipe->commands->next;
    }
    return false;
}