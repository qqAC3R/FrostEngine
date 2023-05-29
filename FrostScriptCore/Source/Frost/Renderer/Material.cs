using System;
using System.Runtime.CompilerServices;

namespace Frost
{
    // `Material` class is only targeted for meshes which contain PBR properties and textures
    public class Material
    {
        public Vector3 AlbedoColor
        {
            get
            {
                GetAlbedoColor_Native(m_UnmanagedInstance, out Vector3 result);
                return result;
            }

            set => SetAlbedoColor_Native(m_UnmanagedInstance, ref value);
        }

        public float Metalness
        {
            get
            {
                GetMetalness_Native(m_UnmanagedInstance, out float result);
                return result;
            }

            set => SetMetalness_Native(m_UnmanagedInstance, value);
        }

        public float Roughness
        {
            get
            {
                GetRoughness_Native(m_UnmanagedInstance, out float result);
                return result;
            }

            set => SetRoughness_Native(m_UnmanagedInstance, value);
        }

        public float Emission
        {
            get
            {
                GetEmission_Native(m_UnmanagedInstance, out float result);
                return result;
            }

            set => SetEmission_Native(m_UnmanagedInstance, value);
        }

        public Texture2D AlbedoTexture
        {
            get
            {
                IntPtr result = GetTextureByString_Native(m_UnmanagedInstance, "Albedo");
                return new Texture2D(result);
            }

            set => SetTextureByString_Native(m_UnmanagedInstance, "Albedo", value.m_UnmanagedInstance);
        }

        public Texture2D MetalnessTexture
        {
            get
            {
                IntPtr result = GetTextureByString_Native(m_UnmanagedInstance, "Metalness");
                return new Texture2D(result);
            }

            set => SetTextureByString_Native(m_UnmanagedInstance, "Metalness", value.m_UnmanagedInstance);
        }

        public Texture2D RoughnessTexture
        {
            get
            {
                IntPtr result = GetTextureByString_Native(m_UnmanagedInstance, "Roughness");
                return new Texture2D(result);
            }

            set => SetTextureByString_Native(m_UnmanagedInstance, "Roughness", value.m_UnmanagedInstance);
        }

        public Texture2D NormalTexture
        {
            get
            {
                IntPtr result = GetTextureByString_Native(m_UnmanagedInstance, "Normal");
                return new Texture2D(result);
            }

            set => SetTextureByString_Native(m_UnmanagedInstance, "Normal", value.m_UnmanagedInstance);
        }

        internal Material(IntPtr unmanagedInstance)
        {
            m_UnmanagedInstance = unmanagedInstance;
        }

        ~Material()
        {
            Destructor_Native(m_UnmanagedInstance);
        }

        internal IntPtr m_UnmanagedInstance;


        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Destructor_Native(IntPtr unmanagedInstance);


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAlbedoColor_Native(IntPtr unmanagedInstance, out Vector3 outAlbedoColor);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAlbedoColor_Native(IntPtr unmanagedInstance, ref Vector3 inAlbedoColor);


        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMetalness_Native(IntPtr unmanagedInstance, out float outMetalness);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMetalness_Native(IntPtr unmanagedInstance, float inMetalness);


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRoughness_Native(IntPtr unmanagedInstance, out float outRoughness);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRoughness_Native(IntPtr unmanagedInstance, float inRoughness);
        
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetEmission_Native(IntPtr unmanagedInstance, out float outEmission);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEmission_Native(IntPtr unmanagedInstance, float inEmission);


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetTextureByString_Native(IntPtr unmanagedInstance, string textureType);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTextureByString_Native(IntPtr unmanagedInstance, string textureType, IntPtr inTexturePtr);


    }
}
