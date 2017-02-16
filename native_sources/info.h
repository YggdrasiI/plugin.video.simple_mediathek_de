/* Read channel list from end of index file,
 * but ignores the rest.
 */
int info_read(
        search_workspace_t *p_i_ws)
;

void info_print(
        int fd,
        search_workspace_t *p_i_ws)
;

/* Same output like info_print but after indexing step
 */
void info_print(
        int fd,
        search_workspace_t *p_i_ws)
;
