using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Frost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Position;
        public Vector3 Rotation;
        public Vector3 Scale;

        public Vector3 Up { get { return new Quaternion(Rotation) * Vector3.Up; } }
        public Vector3 Right { get { return new Quaternion(Rotation) * Vector3.Right; } }
        public Vector3 Forward { get { return new Quaternion(Rotation) * Vector3.Forward; } }

        public void LookAt(Vector3 center, Vector3 eye)
        {
            LookAt_Native(out this, ref center, ref eye);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void LookAt_Native(out Transform outTransform, ref Vector3 center, ref Vector3 eye);
    }
}
