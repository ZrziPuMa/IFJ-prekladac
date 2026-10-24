/**
 * @file main.c
 * @brief Hlavní program překladače IFJcode26.
 * @author František Bilavčík (<xbilavf00>)
 */
int main(void) {
    ASTNode *root = NULL;
    ErrorCode err = parse_program(&root); // 0, 1, 2, 99 při chybě root == NULL, nic nezůstává
    if (err == ERR_OK)
        err = check_semantics(
            root); // 0, 3–7, 10, 99 strom při chybě zůstává celý, vytváří i maže symbtable
    if (err == ERR_OK)
        err = generate_code(root); // 0, 99 tiskne na stdout, ast jen čte
    ast_free(root);                // vždy
    return err;
}