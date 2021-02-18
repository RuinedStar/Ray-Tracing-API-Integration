
/*
    pbrt source code is Copyright(c) 1998-2015
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_CORE_GEOMETRY_H
#define PBRT_CORE_GEOMETRY_H

// core/geometry.h*


template <typename T>
inline bool isNaN(const T x) {
    return std::isnan(x);
}
template <>
inline bool isNaN(const int x) {
    return false;
}

template <typename T>
class Vector3 {
public:
	// Vector3 Public Methods
	T operator[](int i) const {
		//Assert(i >= 0 && i <= 2);
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}
	T &operator[](int i) {
		//Assert(i >= 0 && i <= 2);
		if (i == 0) return x;
		if (i == 1) return y;
		return z;
	}
	Vector3() { x = y = z = 0; }
	Vector3(T x, T y, T z) : x(x), y(y), z(z) { /*Assert(!HasNaNs());*/ }
	bool HasNaNs() const { return isNaN(x) || isNaN(y) || isNaN(z); }
#ifndef NDEBUG
	// The default versions of these are fine for release builds; for debug
	// we define them so that we can add the //Assert checks.
	Vector3(const Vector3<T> &v) {
		//Assert(!v.HasNaNs());
		x = v.x;
		y = v.y;
		z = v.z;
	}

	Vector3<T> &operator=(const Vector3<T> &v) {
		//Assert(!v.HasNaNs());
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}
#endif  // !NDEBUG
	friend std::ostream &operator<<(std::ostream &os, const Vector3<T> &v) {
		os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
		return os;
	}
	Vector3<T> operator+(const Vector3<T> &v) const {
		//Assert(!v.HasNaNs());
		return Vector3(x + v.x, y + v.y, z + v.z);
	}
	Vector3<T> &operator+=(const Vector3<T> &v) {
		//Assert(!v.HasNaNs());
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	Vector3<T> operator-(const Vector3<T> &v) const {
		//Assert(!v.HasNaNs());
		return Vector3(x - v.x, y - v.y, z - v.z);
	}
	Vector3<T> &operator-=(const Vector3<T> &v) {
		//Assert(!v.HasNaNs());
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	bool operator==(const Vector3<T> &v) const {
		return x == v.x && y == v.y && z == v.z;
	}
	bool operator!=(const Vector3<T> &v) const {
		return x != v.x || y != v.y || z != v.z;
	}
	template <typename U>
	Vector3<T> operator*(U s) const {
		return Vector3<T>(s * x, s * y, s * z);
	}
	template <typename U>
	Vector3<T> &operator*=(U s) {
		//Assert(!isNaN(s));
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	template <typename U>
	Vector3<T> operator/(U f) const {
		//Assert(f != 0);
		float inv = (float)1 / f;
		return Vector3<T>(x * inv, y * inv, z * inv);
	}

	template <typename U>
	Vector3<T> &operator/=(U f) {
		//Assert(f != 0);
		float inv = (float)1 / f;
		x *= inv;
		y *= inv;
		z *= inv;
		return *this;
	}
	Vector3<T> operator-() const { return Vector3<T>(-x, -y, -z); }
	float LengthSquared() const { return x * x + y * y + z * z; }
	float Length() const { return std::sqrt(LengthSquared()); }

	// Vector3 Public Data
	T x, y, z;
};

typedef Vector3<float> Vector3f;
typedef Vector3<int> Vector3i;

template <typename T>
class Point3 {
  public:
    // Point3 Public Methods
    Point3() { x = y = z = 0; }
    Point3(T x, T y, T z) : x(x), y(y), z(z) { /*Assert(!HasNaNs());*/ }
    template <typename U>
    explicit Point3(const Point3<U> &p)
        : x((T)p.x), y((T)p.y), z((T)p.z) {
        //Assert(!HasNaNs());
    }
    template <typename U>
    explicit operator Vector3<U>() const {
        return Vector3<U>(x, y, z);
    }
#ifndef NDEBUG
    Point3(const Point3<T> &p) {
        //Assert(!p.HasNaNs());
        x = p.x;
        y = p.y;
        z = p.z;
    }

    Point3<T> &operator=(const Point3<T> &p) {
        //Assert(!p.HasNaNs());
        x = p.x;
        y = p.y;
        z = p.z;
        return *this;
    }
#endif  // !NDEBUG
    friend std::ostream &operator<<(std::ostream &os, const Point3<T> &p) {
        os << "[" << p.x << ", " << p.y << ", " << p.z << "]";
        return os;
    }
    Point3<T> operator+(const Vector3<T> &v) const {
        //Assert(!v.HasNaNs());
        return Point3<T>(x + v.x, y + v.y, z + v.z);
    }
    Point3<T> &operator+=(const Vector3<T> &v) {
        //Assert(!v.HasNaNs());
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    Vector3<T> operator-(const Point3<T> &p) const {
        //Assert(!p.HasNaNs());
        return Vector3<T>(x - p.x, y - p.y, z - p.z);
    }
    Point3<T> operator-(const Vector3<T> &v) const {
        //Assert(!v.HasNaNs());
        return Point3<T>(x - v.x, y - v.y, z - v.z);
    }
    Point3<T> &operator-=(const Vector3<T> &v) {
        //Assert(!v.HasNaNs());
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    Point3<T> &operator+=(const Point3<T> &p) {
        //Assert(!p.HasNaNs());
        x += p.x;
        y += p.y;
        z += p.z;
        return *this;
    }
    Point3<T> operator+(const Point3<T> &p) const {
        //Assert(!p.HasNaNs());
        return Point3<T>(x + p.x, y + p.y, z + p.z);
    }
    template <typename U>
    Point3<T> operator*(U f) const {
        return Point3<T>(f * x, f * y, f * z);
    }
    template <typename U>
    Point3<T> &operator*=(U f) {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    template <typename U>
    Point3<T> operator/(U f) const {
        float inv = (float)1 / f;
        return Point3<T>(inv * x, inv * y, inv * z);
    }
    template <typename U>
    Point3<T> &operator/=(U f) {
        float inv = (float)1 / f;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }
    T operator[](int i) const {
        //Assert(i >= 0 && i <= 2);
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    T &operator[](int i) {
        //Assert(i >= 0 && i <= 2);
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }
    bool operator==(const Point3<T> &p) const {
        return x == p.x && y == p.y && z == p.z;
    }
    bool operator!=(const Point3<T> &p) const {
        return x != p.x || y != p.y || z != p.z;
    }
    bool HasNaNs() const { return isNaN(x) || isNaN(y) || isNaN(z); }
    Point3<T> operator-() const { return Point3<T>(-x, -y, -z); }

    // Point3 Public Data
    T x, y, z;
};

typedef Point3<float> Point3f;
typedef Point3<int> Point3i;

template <typename T>
class Bounds3 {
  public:
    // Bounds3 Public Methods
    Bounds3() {
        T minNum = std::numeric_limits<T>::lowest();
        T maxNum = std::numeric_limits<T>::max();
        pMin = Point3<T>(maxNum, maxNum, maxNum);
        pMax = Point3<T>(minNum, minNum, minNum);
    }
    Bounds3(const Point3<T> &p) : pMin(p), pMax(p) {}
    Bounds3(const Point3<T> &p1, const Point3<T> &p2)
        : pMin(std::min(p1.x, p2.x), std::min(p1.y, p2.y),
               std::min(p1.z, p2.z)),
          pMax(std::max(p1.x, p2.x), std::max(p1.y, p2.y),
               std::max(p1.z, p2.z)) {}
    const Point3<T> &operator[](int i) const;
    Point3<T> &operator[](int i);
    bool operator==(const Bounds3<T> &b) const {
        return b.pMin == pMin && b.pMax == pMax;
    }
    bool operator!=(const Bounds3<T> &b) const {
        return b.pMin != pMin || b.pMax != pMax;
    }
    Point3<T> Corner(int corner) const {
        //Assert(corner >= 0 && corner < 8);
        return Point3<T>((*this)[(corner & 1)].x,
                         (*this)[(corner & 2) ? 1 : 0].y,
                         (*this)[(corner & 4) ? 1 : 0].z);
    }
    Vector3<T> Diagonal() const { return pMax - pMin; }
    T SurfaceArea() const {
        Vector3<T> d = Diagonal();
        return 2 * (d.x * d.y + d.x * d.z + d.y * d.z);
    }
    T Volume() const {
        Vector3<T> d = Diagonal();
        return d.x * d.y * d.z;
    }
    int MaximumExtent() const {
        Vector3<T> d = Diagonal();
        if (d.x > d.y && d.x > d.z)
            return 0;
        else if (d.y > d.z)
            return 1;
        else
            return 2;
    }
    Point3<T> Lerp(const Point3f &t) const {
        return Point3<T>(::Lerp(t.x, pMin.x, pMax.x),
                         ::Lerp(t.y, pMin.y, pMax.y),
                         ::Lerp(t.z, pMin.z, pMax.z));
    }
    Vector3<T> Offset(const Point3<T> &p) const {
        Vector3<T> o = p - pMin;
        if (pMax.x > pMin.x) o.x /= pMax.x - pMin.x;
        if (pMax.y > pMin.y) o.y /= pMax.y - pMin.y;
        if (pMax.z > pMin.z) o.z /= pMax.z - pMin.z;
        return o;
    }
    void BoundingSphere(Point3<T> *center, float *radius) const {
        *center = (pMin + pMax) / 2;
        *radius = Inside(*center, *this) ? Distance(*center, pMax) : 0;
    }
    template <typename U>
    explicit operator Bounds3<U>() const {
        return Bounds3<U>((Point3<U>)pMin, (Point3<U>)pMax);
    }

    // Bounds3 Public Data
    Point3<T> pMin, pMax;
};

typedef Bounds3<float> Bounds3f;
typedef Bounds3<int> Bounds3i;
#include <iterator>

// Geometry Inline Functions
template <typename T>
inline float DistanceSquared(const Point3<T> &p1, const Point3<T> &p2) {
    return (p1 - p2).LengthSquared();
}

template <typename T, typename U>
inline Point3<T> operator*(U f, const Point3<T> &p) {
    //Assert(!p.HasNaNs());
    return p * f;
}

template <typename T>
Point3<T> Lerp(float t, const Point3<T> &p0, const Point3<T> &p1) {
    return (1 - t) * p0 + t * p1;
}

template <typename T>
Point3<T> Min(const Point3<T> &p1, const Point3<T> &p2) {
    return Point3<T>(std::min(p1.x, p2.x), std::min(p1.y, p2.y),
                     std::min(p1.z, p2.z));
}

template <typename T>
Point3<T> Max(const Point3<T> &p1, const Point3<T> &p2) {
    return Point3<T>(std::max(p1.x, p2.x), std::max(p1.y, p2.y),
                     std::max(p1.z, p2.z));
}

template <typename T>
Point3<T> Floor(const Point3<T> &p) {
    return Point3<T>(std::floor(p.x), std::floor(p.y), std::floor(p.z));
}

template <typename T>
Point3<T> Ceil(const Point3<T> &p) {
    return Point3<T>(std::ceil(p.x), std::ceil(p.y), std::ceil(p.z));
}

template <typename T>
Point3<T> Abs(const Point3<T> &p) {
    return Point3<T>(std::abs(p.x), std::abs(p.y), std::abs(p.z));
}
template <typename T>
inline const Point3<T> &Bounds3<T>::operator[](int i) const {
    //Assert(i == 0 || i == 1);
    return (i == 0) ? pMin : pMax;
}

template <typename T>
inline Point3<T> &Bounds3<T>::operator[](int i) {
    //Assert(i == 0 || i == 1);
    return (i == 0) ? pMin : pMax;
}

template <typename T>
Bounds3<T> Union(const Bounds3<T> &b, const Point3<T> &p) {
    return Bounds3<T>(
        Point3<T>(std::min(b.pMin.x, p.x), std::min(b.pMin.y, p.y),
                  std::min(b.pMin.z, p.z)),
        Point3<T>(std::max(b.pMax.x, p.x), std::max(b.pMax.y, p.y),
                  std::max(b.pMax.z, p.z)));
}

template <typename T>
Bounds3<T> Union(const Bounds3<T> &b1, const Bounds3<T> &b2) {
    return Bounds3<T>(Point3<T>(std::min(b1.pMin.x, b2.pMin.x),
                                std::min(b1.pMin.y, b2.pMin.y),
                                std::min(b1.pMin.z, b2.pMin.z)),
                      Point3<T>(std::max(b1.pMax.x, b2.pMax.x),
                                std::max(b1.pMax.y, b2.pMax.y),
                                std::max(b1.pMax.z, b2.pMax.z)));
}

template <typename T>
Bounds3<T> Intersect(const Bounds3<T> &b1, const Bounds3<T> &b2) {
    return Bounds3<T>(Point3<T>(std::max(b1.pMin.x, b2.pMin.x),
                                std::max(b1.pMin.y, b2.pMin.y),
                                std::max(b1.pMin.z, b2.pMin.z)),
                      Point3<T>(std::min(b1.pMax.x, b2.pMax.x),
                                std::min(b1.pMax.y, b2.pMax.y),
                                std::min(b1.pMax.z, b2.pMax.z)));
}

template <typename T>
bool Overlaps(const Bounds3<T> &b1, const Bounds3<T> &b2) {
    bool x = (b1.pMax.x >= b2.pMin.x) && (b1.pMin.x <= b2.pMax.x);
    bool y = (b1.pMax.y >= b2.pMin.y) && (b1.pMin.y <= b2.pMax.y);
    bool z = (b1.pMax.z >= b2.pMin.z) && (b1.pMin.z <= b2.pMax.z);
    return (x && y && z);
}

template <typename T>
bool Inside(const Point3<T> &p, const Bounds3<T> &b) {
    return (p.x >= b.pMin.x && p.x <= b.pMax.x && p.y >= b.pMin.y &&
            p.y <= b.pMax.y && p.z >= b.pMin.z && p.z <= b.pMax.z);
}

template <typename T>
bool InsideExclusive(const Point3<T> &p, const Bounds3<T> &b) {
    return (p.x >= b.pMin.x && p.x < b.pMax.x && p.y >= b.pMin.y &&
            p.y < b.pMax.y && p.z >= b.pMin.z && p.z < b.pMax.z);
}

template <typename T, typename U>
inline Bounds3<T> Expand(const Bounds3<T> &b, U delta) {
    return Bounds3<T>(b.pMin - Vector3<T>(delta, delta, delta),
                      b.pMax + Vector3<T>(delta, delta, delta));
}

#endif  // PBRT_CORE_GEOMETRY_H
