# 项目介绍
ext4文件系统误删除文件恢复工具

## 支持的平台

Linux ext4

# 快速上手     Getting Started

## 环境要求
Linux

## 前提条件


## 操作步骤
### 使用方法
```
ext4recover /dev/sdx
# 假设/dev/sdx是要恢复数据的设备，执行上述命令后，会将能恢复的已删除文件恢复到RECOVER子目录下。
```
### 编译方法
```
yum install -y e2fsprogs-devel libcom_err-devel
cd src
make
```

## 常见问题
#### 原理:
ext4在删除包含多级extent的文件时，会调用ext4_ext_rm_idx 删除下一级， 上一级本身只是eh_entries减一，
但是block位置没修改，同时inode外部的extent块信息清空后没有落盘。所以包含多级extent的文件删除后可以利用这些信息恢复。
只包含一级extent时，只调用ext4_ext_rm_leaf,会修改对应index的eh_entries, 并且每一个entry的block位置被置0， 长度置0,
所以只包含一级extent的文件无法恢复.

如果文件不存在碎片,单文件大于496M就会使用多级extent, 能恢复。
当存在碎片的情况，文件就算很小也可能占用4个以上extent, 因此这些高度碎片化的小文件也能恢复

背景信息：ext4_ext_rm_idx删除的外部extent数据没落盘其实算是一个bug, 只不过当前内核社区还没修复, 
brookxu同学提了补丁：https://lkml.org/lkml/2020/3/5/1248

本工具原始版本是zorrozou所写，curuwang从e2fsprogs代码中剥离并补充原理说明。

为什么说是bug呢? 因为实际上内核代码里面已经将叶子extent清空了(
rm后，直接dd读底层的extent,发现已清0, 但是dd使用iflag=direct绕过pagecache从块设备发现信息有残留,
证明只是把页面写了没有标记未脏，所以没落盘)

文件被删除后, inode里面的信息发生了如下变化:
1. 文件大小,文件链接数变为0
2. 文件dtime,ctime,atime,mtime被设置为删除时间
3. 上述的extent信息变化
4. 其他...


## 行为准则    
代码层面需严格按照开源治理的要求。

### 一句话介绍

根据腾讯代码规范，评估开源项目和协同Oteam项目的代码规范度，提升代码可读性，降低技术交流成本。

### html

1. 编码：所有编码均采用xhtml，标签必须闭合，属性值用双引号包括，编码统一为utf-8。

2. 语义化：语义化html，正确使用标签，充分利用无兼容性问题的html自身标签。

3. 文件命名：命名以中文命名，依实际模块命名，如同一模块以_&title&_来组合命名，以方便添加功能时查找对应页面。

4. 文件头部head的内容
    - title，需要添加标题
    - 编码charset=UTF-8
    - 缓存：
        - Content=’-1’表示立即过期。
    - 添加description、keywords内容
    - Robots content部分有四个指令选项：index、noindex、follow、nofollow，用‘，’分隔，如：
    - 在head标签内引入css文件，有助于页面渲染
    - 引入样式文件或JavaScript文件时， 须略去默认类型声明.
    - 页脚引入javascript文件
    - 连接地址标签：书写链接地址时，避免重定向，如href=”http://www.example.com/”，需在地址后面加‘/’
    - 尽可能减少div嵌套，如：根据重要性使用h1-6标签，段落使用p，列表使用ul，内联元素中不可嵌套块级元素，为含有描述性表单元素(input，tetarea)添加label

5. 图片
    - 能以背景形式呈现的图片，尽量写入css样式中
    - 区分作为内容的图片和作为背景的图片，作为背景的图片采用Css sprite技术，放在一张大图里
    - 重要图片必须加上alt属性，给重要的元素和截断的元素加上title

6. 注释：给区块代码及重要功能加上注释，方便后台添加功能
7. 转义字符：特殊符号使用转义字符
8. 页面架构时考虑扩展性

### CSS

1. 编码统一为utf-8

2. Class与id的使用：
    - Id:具有唯一性，是父级的，用于标识页面上的特定元素，如header/footer/wrapper/left/right之类
    - Class:可以重复使用，是子级的，可用于页面上任意多个元素
    - 命名：以小写英文字母、数字、下划线组合命名，避免使用中文拼音，尽量使用简易的单词组合，避免使用拼音，采用驼峰命名法和划线命名法，提高可读性，如：dropMenu、sub_nav_menu、drop-menu等。为JavaScript预留钩子的命名， 以 js_ 起始， 比如:js_hide， js_show

3. 书写代码前，考虑样式重复利用率，充分利用html自身属性及样式继承原理减少代码量，代码建议单行书写，利于后期管理

4. 图片:
    - 命名：小写英文字母、数字、_ 的组合，使用有意义的名称或英文简写，最好不要使用汉语拼音，区分大写字。
    - 使用sprite技术， 减小http请求，sprite按模块制作
5. 书写顺序：保证同类型属性写在一起，一般遵循布局定位属性–>自身属性–>文本属性–>其他属性的书写格式
    - 书写顺序规则
        - 定位属性(比如：display， position， float， clear， visibility， table-layout等)
        - 自身属性(比如：width， height， margin， padding， border等)
        - 文本属性(比如：font， line-height， text-align， text-indent， vertical-align等)
        - 其他属性(比如：color， background， opacity， cursor，content， list-style， quotes等)
    - 缩进：统一使用tab进行缩进

6. 样式表中中文字体名，最好转成unicode码，以避免编码错误时乱码。
7. 减少影响性能的属性，如：position，float
8. 为大区块样式添加注释，小区块适量注释。

### Javascript部分

1. 代码规范须通过腾讯云ESLint的设定集合的规范
2. 代码安全须通过啄木鸟（定制规则） +codecc（扫描）+邮件通知+工蜂Git（修复）的检查
3. 圈复杂度须通过CodeCC的工具的检查，每个函数圈复杂度需小于等于20



## 如何加入 
联系curuwang申请权限即可

## 团队介绍
云服务器技术支持团队，主要对接CVM/CBS/CFS/云API/黑石等计算产品的专业技术支持工作。
以系统技术为支撑，对内跟进最终产品问题促进产品改进，对外为提供专业技术服务解决客户问题。

同时，团队还拥有大量与客户打交道经验丰富的自营、腾云悦智的同事。
他们深扎与客户，了解客户的需求、行业的特点、不同的视角
给予团队更多元的思考方向与良好的服务口碑。