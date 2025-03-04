#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "sceneStructs.h"
#include "utilities.h"

/**
 * Handy-dandy hash function that provides seeds for random number generation.
 */
__host__ __device__ inline unsigned int utilhash(unsigned int a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// CHECKITOUT
/**
 * Compute a point at parameter value `t` on ray `r`.
 * Falls slightly short so that it doesn't intersect the object it's hitting.
 */
__host__ __device__ glm::vec3 getPointOnRay(Ray r, float t) {
    return r.origin + (t - .0001f) * glm::normalize(r.direction);
}

/**
 * Multiplies a mat4 and a vec4 and returns a vec3 clipped from the vec4.
 */
__host__ __device__ glm::vec3 multiplyMV(glm::mat4 m, glm::vec4 v) {
    return glm::vec3(m * v);
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed cube. Untransformed,
 * the cube ranges from -0.5 to 0.5 in each axis and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float boxIntersectionTest(Geom box, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    Ray q;
    q.origin    =                multiplyMV(box.inverseTransform, glm::vec4(r.origin   , 1.0f));
    q.direction = glm::normalize(multiplyMV(box.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float tmin = -1e38f;
    float tmax = 1e38f;
    glm::vec3 tmin_n;
    glm::vec3 tmax_n;
    for (int xyz = 0; xyz < 3; ++xyz) {
        float qdxyz = q.direction[xyz];
        /*if (glm::abs(qdxyz) > 0.00001f)*/ {
            float t1 = (-0.5f - q.origin[xyz]) / qdxyz;
            float t2 = (+0.5f - q.origin[xyz]) / qdxyz;
            float ta = glm::min(t1, t2);
            float tb = glm::max(t1, t2);
            glm::vec3 n;
            n[xyz] = t2 < t1 ? +1 : -1;
            if (ta > 0 && ta > tmin) {
                tmin = ta;
                tmin_n = n;
            }
            if (tb < tmax) {
                tmax = tb;
                tmax_n = n;
            }
        }
    }

    if (tmax >= tmin && tmax > 0) {
        outside = true;
        if (tmin <= 0) {
            tmin = tmax;
            tmin_n = tmax_n;
            outside = false;
        }
        intersectionPoint = multiplyMV(box.transform, glm::vec4(getPointOnRay(q, tmin), 1.0f));
        normal = glm::normalize(multiplyMV(box.invTranspose, glm::vec4(tmin_n, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }
    return -1;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed sphere. Untransformed,
 * the sphere always has radius 0.5 and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float sphereIntersectionTest(Geom sphere, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    float radius = .5;

    glm::vec3 ro = multiplyMV(sphere.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(sphere.inverseTransform, glm::vec4(r.direction, 0.0f)));

    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    float vDotDirection = glm::dot(rt.origin, rt.direction);
    float radicand = vDotDirection * vDotDirection - (glm::dot(rt.origin, rt.origin) - powf(radius, 2));
    if (radicand < 0) {
        return -1;
    }

    float squareRoot = sqrt(radicand);
    float firstTerm = -vDotDirection;
    float t1 = firstTerm + squareRoot;
    float t2 = firstTerm - squareRoot;

    float t = 0;
    if (t1 < 0 && t2 < 0) {
        return -1;
    } else if (t1 > 0 && t2 > 0) {
        t = min(t1, t2);
        outside = true;
    } else {
        t = max(t1, t2);
        outside = false;
    }

    glm::vec3 objspaceIntersection = getPointOnRay(rt, t);

    intersectionPoint = multiplyMV(sphere.transform, glm::vec4(objspaceIntersection, 1.f));
    normal = glm::normalize(multiplyMV(sphere.invTranspose, glm::vec4(objspaceIntersection, 0.f)));
    if (!outside) {
        normal = -normal;
    }

    return glm::length(r.origin - intersectionPoint);
}

__host__ __device__ float triangleInteractionTest(Geom mesh, Ray r,
    glm::vec3& intersectionPoint, 
    glm::vec3& vertex1, glm::vec3& vertex2, glm::vec3& vertex3,
    glm::vec3& normal1, glm::vec3& normal2, glm::vec3& normal3, glm::vec3& normal, bool& outside) {
    
    glm::vec3 ro = multiplyMV(mesh.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(mesh.inverseTransform, glm::vec4(r.direction, 0.0f)));

    //Transformed Ray
    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    glm::vec3 bary_coords{ 0.f };
    bool is_hit = glm::intersectRayTriangle(rt.origin, rt.direction, vertex1, vertex2, vertex3, bary_coords);

    if (!is_hit) {
        return -1;
    }

    glm::vec3 bary_position = (1.f - bary_coords.x - bary_coords.y) * vertex1 + bary_coords.x * vertex2 + bary_coords.y * vertex3;
    intersectionPoint = multiplyMV(mesh.transform, glm::vec4(bary_position, 1.f));

    glm::vec3 n0;
    glm::vec3 n1;
    glm::vec3 n2;

    //Some obj file does not have "vn" infos
    bool has_normal = (glm::length(normal1) != 0) && (glm::length(normal2) != 0) && (glm::length(normal3) != 0);

    n0 = has_normal ? normal1 : glm::normalize(glm::cross(vertex2 - vertex1, vertex3 - vertex1));
    n1 = has_normal ? normal2 : glm::normalize(glm::cross(vertex1 - vertex2, vertex3 - vertex2));
    n2 = has_normal ? normal3 : glm::normalize(glm::cross(vertex1 - vertex3, vertex2 - vertex3));

    //Use smoothed normal (no triangulated mesh look)
    float S = 0.5f * glm::length(glm::cross(vertex1 - vertex2, vertex3 - vertex2));
    float S0 = 0.5f * glm::length(glm::cross(vertex2 - bary_position, vertex3 - bary_position));
    float S1 = 0.5f * glm::length(glm::cross(vertex1 - bary_position, vertex3 - bary_position));
    float S2 = 0.5f * glm::length(glm::cross(vertex1 - bary_position, vertex2 - bary_position));
    glm::vec3 smoothNormal = glm::normalize(n0 * S0 / S + n1 * S1 / S + n2 * S2 / S);

    normal = glm::normalize(multiplyMV(mesh.invTranspose, glm::vec4(smoothNormal, 0.f)));

    if (glm::dot(normal, r.direction) > 0) {
        normal = -normal;
        outside = false;
    }

    return glm::length(r.origin - intersectionPoint);
}
