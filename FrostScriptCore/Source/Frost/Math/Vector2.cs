using System;
using System.Runtime.InteropServices;

namespace Frost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X;
        public float Y;

        public Vector2(float scalar)
        {
            X = Y = scalar;
        }

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }
        public Vector2(Vector3 vector)
        {
            X = vector.X;
            Y = vector.Y;
        }

        public void Clamp(Vector2 min, Vector2 max)
        {
            X = Mathf.Clamp(X, min.X, max.X);
            Y = Mathf.Clamp(Y, min.Y, max.Y);
        }

        public float Distance(Vector3 other)
        {
            return (float)Math.Sqrt(Math.Pow(other.X - X, 2) +
                                    Math.Pow(other.Y - Y, 2));
        }

        public static float Distance(Vector3 p1, Vector3 p2)
        {
            return (float)Math.Sqrt(Math.Pow(p2.X - p1.X, 2) +
                                    Math.Pow(p2.Y - p1.Y, 2));
        }

        public static Vector2 operator -(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X - right.X, left.Y - right.Y);
        }

        public static Vector2 operator -(Vector2 vector)
        {
            return new Vector2(-vector.X, -vector.Y);
        }

        public override string ToString()
        {
            return "Vector2[" + X + ", " + Y + "]";
        }

        public float Length()
        {
            return (float)Math.Sqrt(X * X + Y * Y);
        } 

    }
}
