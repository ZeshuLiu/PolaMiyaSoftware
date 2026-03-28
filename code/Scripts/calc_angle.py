import math
import numpy as np

# 圆心 (120, 394)，半径 r=208
center_x, center_y = 120, 394
r = 208

# x 横坐标范围：0 到 240
x_label = np.arange(0, 240, 0.2).tolist()

# 已知 x 求 y：(x-120)^2 + (y-394)^2 = 208^2
# y = 394 - sqrt(208^2 - (x-120)^2) (下半圆)
y_lable = [center_y - math.sqrt(r**2 - (i - center_x)**2) for i in x_label]

# 生成 [(x, y)] 列表
points = [(x, y) for x, y in zip(x_label, y_lable)]

points_int = [(int(x), int(y)) for x, y in zip(x_label, y_lable)]

# 同一个 x 只保留最大的 y
points_dict = {}
for x, y in points_int:
    if x not in points_dict or y > points_dict[x]:
        points_dict[x] = y

points_int_s = sorted(points_dict.items())  # [(x, y), ...] 按 x 排序
# print(points_int_s)

# 从角度计算：竖直向上为 0 度，顺时针为正
angles = np.arange(-35, 35, 0.5).tolist()  # 角度范围 0-360 度
points_angle = []
for angle in angles:
    rad = math.radians(angle)
    # 竖直向上为 0 度：x = cx + r*sin(θ), y = cy - r*cos(θ)
    x = center_x + r * math.sin(rad)
    y = center_y - r * math.cos(rad)
    points_angle.append((x, y))

# 圆整为整数
points_angle_int = [(int(round(x)), int(round(y))) for x, y in points_angle]

# 变成 5x28 的矩阵
matrix = np.array(points_angle_int[:140]).reshape(5, 28, 2)

# print(points_angle_int)
# print(matrix)

cordsplit = [points_angle_int[i*28:(i+1)*28] for i in range(5)]

# 提取 x 坐标
xlist = [[point[0]-15 for point in row] for row in cordsplit]
ylist = [[point[1] for point in row] for row in cordsplit]
print(xlist)
print(ylist)