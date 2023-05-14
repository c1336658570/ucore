# uCore-Tutorial-Code
## 运行
```bash
make -C user clean  #清除user仓库的编译结果
#等价于
cd user
make clean
cd ..
#可以通过 make user 生成用户程序，最终将 .bin 文件放在 user/target/bin 目录下
make user BASE=1 CHAPTER=2
make run
#也可以直接运行打包好的测试程序。make test 会完成　make user 和 make run 两个步骤（自动设置 CHAPTER）
make test BASE=1
```