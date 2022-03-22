/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 guruthree, Blayzeing
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

// thanks to @Blayzeing for his help with this!

struct Vector3 {
    float x, y, z;
};

struct Matrix3{
    float ii, ij, ik,
          ji, jj, jk,
          ki, kj, kk;
    
    Matrix3 multiply(Matrix3);
    Vector3 preMultiply(Vector3);

    static Matrix3 getRotationMatrix(float alpha, float beta, float gamma);
};


Matrix3 Matrix3::multiply(Matrix3 m)
{
    Matrix3 output = {m.ii*ii+m.ji*ij+m.ki*ik, m.ij*ii+m.jj*ij+m.kj*ik, m.ik*ii+m.jk*ij+m.kk*ik,
                      m.ii*ji+m.ji*jj+m.ki*jk, m.ij*ji+m.jj*jj+m.kj*jk, m.ik*ji+m.jk*jj+m.kk*jk,
                      m.ii*ki+m.ji*kj+m.ki*kk, m.ij*ki+m.jj*kj+m.kj*kk, m.ik*ki+m.jk*kj+m.kk*kk};
    return output;
}
Vector3 Matrix3::preMultiply(Vector3 v)
{
    Vector3 output = {v.x*ii + v.y*ij + v.z*ik,
                      v.x*ji + v.y*jj + v.z*jk,
                      v.x*ki + v.y*kj + v.z*kk};
    return output;
}

// rotation matrix, angles about rotate(x, y, z)
Matrix3 Matrix3::getRotationMatrix(float alpha, float beta, float gamma) 
{
    Matrix3 Rz = {cosf(gamma), -sinf(gamma), 0,
                  sinf(gamma),  cosf(gamma), 0,
                            0,            0, 1};
    Matrix3 Ry = { cosf(beta), 0, sinf(beta),
                            0, 1,          0,
                  -sinf(beta), 0, cosf(beta)};
    Matrix3 Rx = {  1,           0,            0,
                    0, cosf(alpha), -sinf(alpha),
                    0, sinf(alpha),  cosf(alpha)};
    return Rz.multiply(Ry.multiply(Rx));
}

