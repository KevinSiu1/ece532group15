# -*- coding: utf-8 -*-
"""
Created on Fri Feb 10 01:10:53 2017

@author: shang
"""

import cv2
import numpy as np

def Pyramid(img):
    height, width, channels = img.shape

    # 4x4 pixels = 1 block
    new_height = height / 4
    new_width = width / 4
    thumbnail = np.zeros((new_height, new_width, channels), np.uint8)

    # BGR images
    for i in range(0, new_height):
        for j in range(0, new_width):
            blue = img[i*4, j*4, 0]
            green = img[i*4, j*4, 1]
            red = img[i*4, j*4, 2]
            thumbnail[i, j] = (blue, green, red)
    return thumbnail


def ColorDetect(thumbnail, color):
    height, width, channels = thumbnail.shape
    sum_h = 0
    sum_w = 0
    counter = 0

    for i in range(0, height):
        for j in range(0, width):
            blue = thumbnail[i, j, 0]
            green = thumbnail[i, j, 1]
            red = thumbnail[i, j, 2]
#            print("%s%s%s," % (format(red, '02x'), format(green, '02x'), format(blue, '02x')))

            if ((color == 'red') and (red >= 130) and (green <= 80) and (blue <= 80)) \
                    or ((color == 'green') and (red <= 80) and (green >= 180) and (blue <= 80)) \
                    or ((color == 'blue') and (red <= 80) and (green <= 80) and (blue >= 180)) \
                    or ((color == 'yellow') and (red >= 200) and (green >= 200) and (blue <= 80)):
                sum_h += i
                sum_w += j
                counter += 1

    if counter != 0 :
        avg_h = int(round(sum_h / counter))
        avg_w = int(round(sum_w / counter))
    else:
        avg_h = -1
        avg_w = -1

    return avg_h, avg_w


def main():

    cap = cv2.VideoCapture('test.mp4') #video file goes here
    i = 0;

    while(cap.isOpened()):
        ret, img = cap.read()

        row_r, col_r = ColorDetect(img, 'red')
#        row_g, col_g = ColorDetect(img, 'green')
#        row_b, col_b = ColorDetect(img, 'blue')
#        row_y, col_y = ColorDetect(img, 'yellow')
        img[row_r, col_r] = (0, 0, 0)
#        img[row_g, col_g] = (0, 0, 0)
#        img[row_b, col_b] = (0, 0, 0)
#        img[row_y, col_y] = (0, 0, 0)
        print('RED: row: ' + str(row_r) + ' col: ' + str(col_r))
#        print('GREEN: row: ' + str(row_g) + ' col: ' + str(col_g))s
#        print('BLUE: row: ' + str(row_b) + ' col: ' + str(col_b))
#        print('YELLOW: row: ' + str(row_y) + ' col: ' + str(col_y))

        file_name = "frame" + str(i) + ".jpg"
        cv2.imwrite(file_name, img)
        i = i + 1

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()

    return

if __name__ == "__main__":
    main()
