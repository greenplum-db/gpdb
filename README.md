# gpdb_ecnu

## 主要思想：
采样：对每个窗口的数据进行采样，然后根据采样后的数据的窗口函数值来进行拟合用户所需要的窗口函数值。

关于该方法的具体介绍和示例请参考

Song G, Qu W, Liu X, et al. Approximate Calculation of Window Aggregate Functions via Global Random Sample[J]. Data Science & Engineering, 2018(2):1-12.


## 范围：
该版本的代码仅对avg进行了优化，其他函数的优化正在继续修改代码。

## 使用方法
(1)使用gpconfig -c enable_sample -v * 指定是否采样， 1 表示采样，0表示不采样

(2)使用gpconfig -c sample_percent -v * 指定采样率


## 修改记录：

(1)修改nodeWindowagg文件：

	(a)添加窗口聚合函数处理分支
		eval_windowaggregates_sample();
	
	(b)修改ExecInitWindowAgg，根据GUC参数调用不同的eval_windowaggregates_*()函数

	
(2)修改execnode.h

	(a)修改WindowAggState结构体，添加新变量

	
(5)修改guc_gp.h

	(a)将enable_ttv、enable_sample参数添加到ConfigureNamesInt_gp中
