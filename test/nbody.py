#!/usr/bin/python

import jrpc
import pygame
import random
import math

random.seed(0)

server = None
server = jrpc.service.SocketProxy(8764, host = "vector57.net") #The server's listening port

pygame.init()
screen_size = 700
screen = pygame.display.set_mode((screen_size, screen_size))
#points = [[0,0,1], [0,1,1], [1,0,1], [1,1,1]]
points = []
point_count = 20
for x in range(point_count):
    for y in range(point_count):
        points.append([1.0 * screen_size * x / point_count, 1.0 * screen_size * y / point_count,
            (random.random() - .5) / 100, (random.random() - .5) / 100,
            1000 * random.random()])
body_count = len(points)


bid, = server.init_simulation(points)
print bid

while True:
    screen.fill((0,0,0))
    for point in points:
        if any([math.isnan(r) for r in point]):
            print point
        pygame.draw.circle(screen, (128,128,128), (int(point[0]), int(point[1])), int(math.pow(point[-1]/200, .5)), 0)

    pygame.display.update()
    points = server.run_simulation(body_count, bid, 1000, 1, 1)
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            exit()
