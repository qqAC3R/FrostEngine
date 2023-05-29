using System;
using System.Runtime.CompilerServices;

namespace Frost
{
    public class Mesh
    {
        public Mesh(string filepath)
        {
            m_UnmanagedInstance = Constructor_Native(filepath);
        }
        internal Mesh(IntPtr unmanagedInstance)
        {
            m_UnmanagedInstance = unmanagedInstance;
        }

        ~Mesh()
        {
            Destructor_Native(m_UnmanagedInstance);
        }

        public Material GetMaterial(int index)
        {
            IntPtr result = GetMaterialByIndex_Native(m_UnmanagedInstance, index);
            if (result == null)
                return null;

            return new Material(result);
        }

        public int GetMaterialCount()
        {
            return GetMaterialCount_Native(m_UnmanagedInstance);
        }


        internal IntPtr m_UnmanagedInstance;


        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern IntPtr Constructor_Native(string filepath);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Destructor_Native(IntPtr unmanagedInstance);


        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern IntPtr GetMaterialByIndex_Native(IntPtr unmanagedInstance, int index);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int GetMaterialCount_Native(IntPtr unmanagedInstance);
    }
}
