# **全天候AI健身私教“智能健身镜”交互系统**

## **摘要**

本项目设计并实现了一套以海思嵌入式平台为核心的“智能健身镜”交互系统，旨在解决家庭健身中专业指导缺失、动作不规范等痛点。系统采用先进的“边缘端-应用协同”架构，利用海思平台强大的AI边缘计算能力，在端侧完成核心的姿态识别与分析任务，并通过Web前端提供丰富的图形交互与反馈，实现了高效、低延迟的智能健身体验。

## **✨ 核心功能与特性**

* **实时姿态追踪与可视化**：通过摄像头捕捉用户动作，实时在屏幕上绘制33个人体关键骨骼点，提供直观的“镜面”反馈。  
* **多动作识别与计数**：内置深蹲、弓步、侧平举等多种动作分析模块，能够自动识别并精准计数标准动作。  
* **智能实时纠错**：基于运动学原理，实时计算关节角度、肢体位置等，当动作不规范时（如“蹲得不够深”），立即通过文字和语音进行提示。  
* **全流程语音交互**：支持通过语音指令控制训练流程，实现完整的免接触操作。  
* **个性化数据分析与报告**：训练结束后，自动生成包含次数、时长、卡路里消耗的总结报告，并提供多维度图表分析，帮助用户复盘提升。  
* **用户配置与数据持久化**：用户可输入体重等个人信息，用于更科学的卡路里计算，数据将保存在本地。

## **📸 应用截图**


<img width="849" height="530" alt="image" src="https://github.com/user-attachments/assets/559a14e5-c009-403a-b2ec-3ce3d8a30a9b" />
<img width="858" height="538" alt="image" src="https://github.com/user-attachments/assets/51b226a8-cf70-49e4-9434-55cac868574b" />
<img width="859" height="545" alt="image" src="https://github.com/user-attachments/assets/54ee958d-01cf-4b50-83a5-ae3acca63c8f" />
<img width="861" height="536" alt="image" src="https://github.com/user-attachments/assets/7c8b79d5-743a-440d-8488-84f2d0d6bc49" />
<img width="864" height="540" alt="image" src="https://github.com/user-attachments/assets/feb4e651-0cc1-4a14-b9e7-f28cd4e29105" />
<img width="866" height="541" alt="image" src="https://github.com/user-attachments/assets/d9935db4-0432-4f2d-b8fd-79454bc9b52c" />
<img width="857" height="546" alt="image" src="https://github.com/user-attachments/assets/4c612e82-b1de-4e2a-a12c-37b91158e59c" />


## **🚀 主要技术特点**

1. **先进的端-应用协同架构 (Edge-Application Architecture)**：将海思嵌入式板作为核心AI处理单元，Web前端作为轻量化的图形渲染与业务逻辑终端。这种架构将计算密集型任务保留在端侧，仅将轻量的结构化结果数据（JSON）传送至应用层，实现了算力与成本的最佳平衡。  
2. **高效的嵌入式AI推理管线 (High-Efficiency Embedded AI Pipeline)**：在海思板上，我们设计了一套基于硬件加速的两阶段AI推理管线。首先通过轻量级YOLOX模型进行人体检测，随后调用轻量化姿态估计网络解算人体关键点。整个过程由NNIE硬件单元全程加速，实现了高帧率、低功耗的实时处理。  
3. **基于结构化数据的插件式运动分析引擎**：前端的运动分析引擎处理的不再是原始视频流，而是来自海思板的结构化骨骼点坐标数据。每种运动的分析算法被封装为独立的TypeScript模块，使得添加新运动变得极其简单，且前端计算负载极低。  
4. **轻量化实时数据通信机制**：前后端通过WebSocket建立持久连接。我们设计的通信协议以轻量化为核心，后端仅广播推理出的结构化坐标数据，相较于传输原始视频流，数据传输量降低了超过99%，极大提升了系统的实时性和响应速度。

## **🛠️ 系统架构**
<img width="865" height="624" alt="image" src="https://github.com/user-attachments/assets/6791d8b0-47db-495f-b927-277de271e89c" />
### **整体架构**

系统主要分为 **端侧设备 (海思平台)** 和 **应用侧 (浏览器/PC)** 两大部分。端侧负责完成所有的AI计算，并将结果（骨骼点JSON数据）通过WebSocket发送给应用侧；应用侧负责业务逻辑处理、数据可视化、用户交互和反向控制指令的发送。

### **硬件系统**
<img width="864" height="535" alt="image" src="https://github.com/user-attachments/assets/0a6d986a-4a3d-4b23-a658-4dad0a96c235" />

系统硬件由**Taurus开发板**（主控芯片Hi3516DV300）和我们自己设计的**WS63舵机控制板**组成。Hi3516DV300负责视频采集、AI推理和主要的通信功能；WS63核心板则通过UART接收指令，控制双轴舵机进行人体跟踪。

| Taurus开发板 | WS63舵机控制板PCB |

<img width="810" height="440" alt="image" src="https://github.com/user-attachments/assets/8b21b106-43d2-4529-bd8d-7e7eaae8978f" />
<img width="881" height="363" alt="image" src="https://github.com/user-attachments/assets/c38a3e5e-3462-4f9a-a251-e8b9b951fc1f" />

**整体硬件实物图**
<img width="827" height="594" alt="image" src="https://github.com/user-attachments/assets/9b9cbbd8-cca3-49df-bcea-2c646acb9edc" />
<img width="881" height="524" alt="image" src="https://github.com/user-attachments/assets/901c808b-a448-47b0-8590-2b15afaec6a0" />


### **软件系统**

本系统的软件部分由三大核心组件构成：

1. **板端应用程序**：基于HiMPP媒体处理软件开发，负责视频采集、AI推理管线、音频处理以及通过UDP/UART的通信控制。  
2. **Python中转服务器**：一个高性能的异步数据桥梁，负责接收海思板通过UDP发送的视频流（用于调试展示），并通过WebSocket将AI结果数据广播给前端应用。  
3. **前端Web应用**：基于 **Vue 3** 和 **TypeScript** 构建，是系统的主要用户交互界面。它通过WebSocket接收实时骨骼点数据，驱动**插件式运动分析引擎**和基于**Pinia**的状态机，完成动作识别、计数、纠错、UI渲染和语音交互的完整闭环。

## **📈 主要性能指标**

| 性能指标 | 指标值 |
| :---- | :---- |
| 视频帧率 | 30FPS |
| 姿态识别延迟 | \< 50ms |
| 动作计数准确率 | \> 95% |
| 动作纠错响应时间 | \< 300ms |
| 关键点检测精度 | \> 98% |
| 内存占用 | \< 500MB |

<img width="639" height="588" alt="image" src="https://github.com/user-attachments/assets/16c8474f-6c8f-40ca-bd78-04776168765f" />

## **💡 主要创新点**

1. **架构创新**：实现了一种“强端弱云”的边缘计算新范式，将核心AI算力深度部署于低成本嵌入式平台，应用端轻量化为纯粹的交互界面，实现了算力不出设备、数据高度压缩，兼顾了成本与性能。  
2. **技术创新**：在行业内创新地采用\*\*“AI结果流”替代“视频流”\*\*的通信方式，仅传输结构化坐标数据，极大优化了系统带宽占用和实时性。  
3. **交互创新**：设计了“姿态锁定+语音确认”的校准流程，交互逻辑由端侧AI直接驱动，构成了感知、决策、反馈的完整闭环免接触交互。  
4. **设计创新**：应用层设计了分层解耦的插件式运动算法引擎，将业务逻辑与底层AI框架高度解耦，使系统功能扩展极为灵活、高效。

## **🔮 未来扩展**

* **多模态传感器融合**：集成心率、压力传感器等，提供更深层次的运动生理学反馈。  
* **端侧AI算法持续升级**：跟进业界前沿的轻量化模型（如YOLOv8n），通过量化、剪枝等手段进一步压榨端侧性能。  
* **构建完整健康生态**：拓展为独立的移动App，建立用户社群、提供定制化课程，升级为一站式智能健康服务平台。

## **致谢**

本项目的成功离不开指导教师的悉心指导，以及海思官方论坛与技术交流群的热心帮助。特别是在解决模型转换过程中的关键算子兼容性问题时，社区工程师的建议给予了我们决定性的启发。

## **参考文献**

\[1\] Google LLC. MediaPipe: Cross-platform, customizable ML solutions for live and streaming media. https://mediapipe.dev, 2023\.  
\[2\] Jocher, G., et al. YOLOv5 by Ultralytics. https://github.com/ultralytics/yolov5, 2023\.  
\[3\] 张俊, 王雷, 刘洋. "基于 MediaPipe 的实时姿态估计与动作识别方法研究." 计算机与现代化, vol. 3, pp. 45-50, 2022\.  
\[4\] 徐建军, 李艳. "智能健身镜的发展趋势与技术架构分析." 人工智能与物联网技术, vol. 5, no. 2, pp. 12-18, 2023\.  
\[5\] 华为技术有限公司. (2019). Hi3516DV300\_PINOUT\_CN \[Excel file\].  
\[6\] 上海海思技术有限公司. HiMPP 媒体处理软件 V4.0 开发参考, 上海：上海海思技术有限公司，2020.
