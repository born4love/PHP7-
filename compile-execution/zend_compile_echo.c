// 编译echo语句的opcode
void zend_compile_echo(zend_ast *ast)
{
  zend_op *opline;
  // 只有一个子节点
  zend_ast *expr_ast = ast->child[0];
  
  znode expr_node;
  zend_compile_expr(&expr_node, expr_ast);
  // 生成1条新的opcode
  opline = zend_emit_op(NULL, ZENDO_ECHO, &expr_node, NULL);
  opline->extended_value = 0;
}
