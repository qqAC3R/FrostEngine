using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Frost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit
    {
        public ulong EntityID { get; internal set; }
        public Vector3 Position { get; internal set; }
        public Vector3 Normal { get; internal set; }
        public float Distance { get; internal set; }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastData
    {
        public Vector3 Origin;
        public Vector3 Direction;
        public float MaxDistance;
        public Type[] RequiredComponents;
    }

    public enum ActorLockFlag : uint
    {
        TranslationX = 1 << 0,
        TranslationY = 1 << 1,
        TranslationZ = 1 << 2,
        TranslationXYZ = TranslationX | TranslationY | TranslationZ,

        RotationX = 1 << 3,
        RotationY = 1 << 4,
        RotationZ = 1 << 5,
        RotationXYZ = RotationX | RotationY | RotationZ
    }

    public static class Physics
    {
        public static Vector3 Gravity
        {
            get
            {
                GetGravity_Native(out Vector3 gravity);
                return gravity;
            }
            set => SetGravity_Native(ref value);
        }

        public static bool Raycast(RaycastData raycastData, out RaycastHit hit) => Raycast_Native(ref raycastData, out hit);

        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, out RaycastHit hit, params Type[] componentFilters)
        {
            s_RaycastData.Origin = origin;
            s_RaycastData.Direction = direction;
            s_RaycastData.MaxDistance = maxDistance;
            s_RaycastData.RequiredComponents = componentFilters;
            return Raycast_Native(ref s_RaycastData, out hit);
        }

        public static Collider[] OverlapBox(Vector3 origin, Vector3 halfSize)
        {
            return OverlapBox_Native(ref origin, ref halfSize);
        }

        public static Collider[] OverlapCapsule(Vector3 origin, float radius, float halfHeight)
        {
            return OverlapCapsule_Native(ref origin, radius, halfHeight);
        }

        public static Collider[] OverlapSphere(Vector3 origin, float radius)
        {
            return OverlapSphere_Native(ref origin, radius);
        }

        private static RaycastData s_RaycastData = new RaycastData();

        // Ray Casting
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Raycast_Native(ref RaycastData raycastData, out RaycastHit hit);

        // Gravity options
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetGravity_Native(out Vector3 gravity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetGravity_Native(ref Vector3 gravity);

        // Overlap geometry
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Collider[] OverlapBox_Native(ref Vector3 origin, ref Vector3 halfSize);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Collider[] OverlapCapsule_Native(ref Vector3 origin, float radius, float halfHeight);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Collider[] OverlapSphere_Native(ref Vector3 origin, float radius);
    }

}
