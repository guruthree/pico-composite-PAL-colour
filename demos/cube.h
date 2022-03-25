/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) size/222 guruthree
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <vector>
#include <algorithm>

#include "discountadafruitgfx.h"
#include "vectormath.h"

struct TriangleDepth {
    float depth;
    uint8_t index;

	bool operator < (const TriangleDepth& rhs) {
		return depth < rhs.depth;
    }
};

class Object {

    public:
        std::vector<Vector3> vt; // verticies for rendering
        std::vector<uint8_t> triangles;
        std::vector<uint8_t> color;

        Object() {}
};

class TriangleRenderer {

    private:
        std::vector<Vector3> vt; // verticies
        std::vector<uint8_t> triangles;
        std::vector<uint8_t> color;

        std::vector<TriangleDepth> tridepths;

        // calculate the distance away of each triangle from the screen
        void calculateDistances() {
            tridepths.clear();
            for (uint8_t i = 0; i < triangles.size(); i+=3) {
                TriangleDepth td;

                // this isn't really general, it assumes triangles are in pairs making up squares
                if (i % 6 == 0) {
                    td.depth = (vt[triangles[i]].z + vt[triangles[i+1]].z + vt[triangles[i+2]].z + 
                        vt[triangles[i+3]].z + vt[triangles[i+4]].z + vt[triangles[i+5]].z) / 6;
                }
                else {
                    td.depth = (vt[triangles[i]].z + vt[triangles[i+1]].z + vt[triangles[i+2]].z + 
                        vt[triangles[i-3]].z + vt[triangles[i-2]].z + vt[triangles[i-1]].z) / 6;
                }

                td.index = i;
                tridepths.push_back(td);
            }

			std::sort(tridepths.begin(), tridepths.end());

        }

    public:
        TriangleRenderer() {}

        static int compare (const void* _a, const void* _b) {
            TriangleDepth *a = (TriangleDepth*)_a;
            TriangleDepth *b = (TriangleDepth*)_b;
            if (a->depth == b->depth) return 0;
            else if (a->depth < b->depth) return -1;
            else return 1;
        }

        void reset() {
            vt.clear();
            triangles.clear();
            color.clear();
        }

        void addObject(Object &obj, int8_t *tbuf) {
            vt.insert(vt.end(), obj.vt.begin(), obj.vt.end());
            triangles.insert(triangles.end(), obj.triangles.begin(), obj.triangles.end());
            color.insert(color.end(), obj.color.begin(), obj.color.end());

			// need to offset the triangles references when adding...
        }

        void render(int8_t *tbuf) {
            calculateDistances();

            for (uint8_t i = 0; i < tridepths.size(); i++) {
                uint8_t idx = tridepths[i].index;

                fillTriangle(tbuf, 
                    vt[triangles[idx]].x, vt[triangles[idx]].y, 
                    vt[triangles[idx+1]].x, vt[triangles[idx+1]].y, 
                    vt[triangles[idx+2]].x, vt[triangles[idx+2]].y, 
                    color[idx], color[idx+1], color[idx+2]);
            }

        }

        /*

        so to render:
        reset()
        addObject(object);
        addObject(object);
        render();

        */
}; // end TriangleRenderer

class Cube : public Object {
    std::vector<Vector3> v; // base verticies before animation
    Vector3 centre;

    float xangle = 0, yangle = 0, zangle = 0;
    float dxangle = 0.05f, dyangle = 0.02f, dzangle = 0.001f;
    Vector3 d = {0.10f, 0.50f, -0.25f}; // dx, dy, dz

    public:
        Cube(float size) {
            v.reserve(8);
            v.push_back({-size/2, -size/2, size/2}); // front bottom left
            v.push_back({ size/2, -size/2,  size/2}); // front bottom right
            v.push_back({ size/2,  size/2,  size/2}); // front top right
            v.push_back({-size/2,  size/2,  size/2}); // front top left
            v.push_back({-size/2, -size/2, -size/2}); // back bottom left
            v.push_back({ size/2, -size/2, -size/2}); // back bottom right
            v.push_back({ size/2,  size/2, -size/2}); // back top right 
            v.push_back({-size/2,  size/2, -size/2}); // back top left

            triangles.reserve(12*3); // 6 faces * 2 triangles per face * 3 coordinates
            triangles = {0, 1, 2, 
                         2, 3, 0,
                         4, 5, 6, 
                         6, 7, 4,
                         0, 3, 4,
                         4, 7, 3,
                         1, 2, 5,
                         5, 6, 2,
                         2, 3, 6,
                         6, 3, 7,
                         1, 0, 5,
                         5, 0, 4
                         };

            color.reserve(12*3); // 6 faces * 2 triangles per face * RGB (3)
            color = {100,   0,   0, 
                     100,   0,   0, 
                       0,   0, 100, 
                       0,   0, 100,
                       0, 100,   0,
                       0, 100,   0,
                     100, 100,   0,
                     100, 100,   0,
                     0, 100,  100,
                     0, 100,  100,
                     100, 0,  100,
                     100, 0,  100
                     };

            centre = {0, 0, 0};
        }

        // animate
        // this also mixes in bits of rendering, because the cube bounces off the screen
        void step() {
            // rotate
            xangle += dxangle;
            yangle += dyangle;
            zangle += dxangle;
            if (xangle >= 360) xangle -= 360;
            if (yangle >= 360) yangle -= 360;
            if (zangle >= 360) zangle -= 360;

            // move
            centre = centre.add(d);

            // bounce off the front and back of the screen
            if (centre.z < -100 || centre.z >= 0) {
                d.z = -d.z;
            }

            Matrix3 rot = Matrix3::getRotationMatrix(xangle, yangle, zangle);

            vt.clear();
            vt.reserve(v.size());
            for (uint8_t i = 0; i < v.size(); i++) {
                // apply effects of movement
                vt.push_back(rot.preMultiply(v[i]));
                vt[i] = vt[i].add(centre);
                // apply perspective
                vt[i] = vt[i].scale(40.0f / (-vt[i].z/2.0 + 40.0f));
                // scale coordinates to screen coordinates*/
                vt[i].x = (vt[i].x/2) + 32;
                vt[i].y += 62;
            }

            // bounce off the sides of the screen
            for (uint8_t i = 0; i < v.size(); i++) {
                if (vt[i].x > 60) {
                    d.x = -abs(d.x);
                    d.z = -abs(d.z); // don't continue to grow on edge of screen
                    break;
                }
                else if (vt[i].x < 2) {
                    d.x = abs(d.x);
                    d.z = -abs(d.z);
                    break;
                }
            }

            // bounce off the top and bottom of the screen
            for (uint8_t i = 0; i < v.size(); i++) {
                if (vt[i].y > 120) {
                    d.y = -abs(d.y);
                    d.z = -abs(d.z);
                    break;
                }
                else if (vt[i].y < 2) {
                    d.y = abs(d.y);
                    d.z = -abs(d.z);
                    break;
               }
            }
        }

        void randomise() {
            centre = { float(rand() % 20) - 10.0f,
                       float(rand() % 40) - 20.0f,
                      -float(rand() % 80) };
            dxangle = rand()*0.05f/RAND_MAX;
            dyangle = rand()*0.05f/RAND_MAX;
            dzangle = rand()*0.05f/RAND_MAX;
            d = {rand()*0.25f/RAND_MAX-0.125f, rand()*0.25f/RAND_MAX-0.125f, -rand()*0.25f/RAND_MAX}; // dx, dy, dz
        }
}; // end Cube
