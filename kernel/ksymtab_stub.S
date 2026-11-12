.section .ksym,"a",@progbits
.balign 8

.globl ksym_table
.type ksym_table, @object
ksym_table:
  .quad 0
  .quad 0
  .quad 0
.size ksym_table, .-ksym_table

.globl ksym_count
.type ksym_count, @object
ksym_count:
  .quad 0
.size ksym_count, 8
