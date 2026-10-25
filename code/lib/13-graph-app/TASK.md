# lib/13-graph-app · 题目（待补）

> 完整题目（主程序的输入输出格式与样例、真题题面）会在写标准答案时补上，届时运行 `./lab.sh sync` 不会覆盖它。
> 下面先摘录 `LEVELS.md` 中本关的任务说明。

### 13 · 图的应用
- **产出**：Prim、Kruskal（用到第 09 关的并查集和堆）；Dijkstra、Floyd；拓扑排序；关键路径
- **小步骤**：① 最小生成树 ② Dijkstra，trace 按王道表格格式打印每一轮的 dist 和 path ③ Floyd，打印每次迭代的矩阵 ④ 拓扑排序 ⑤ 关键路径：输出 ve、vl、e、l 和关键活动
- **通过标准**：真题考法：Dijkstra 某一轮的选择结果、拓扑序列是否唯一、关键路径长度、哪些活动缩短能缩短工期
- **概念卡片**：Prim vs Kruskal；Dijkstra vs Floyd（负权边、时间复杂度）
