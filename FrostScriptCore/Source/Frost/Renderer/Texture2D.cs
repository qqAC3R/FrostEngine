using System;
using System.Runtime.CompilerServices;

namespace Frost
{
    public class Texture2D
    {
        public Texture2D(uint width, uint height)
        {
            Console.WriteLine("Created texture! Constructor1 (Width, Height)");
            m_UnmanagedInstance = Constructor_Native(width, height);
        }
        public Texture2D(string filepath)
        {
            Console.WriteLine("Created texture! Constructor2 (Filepath)");
            m_UnmanagedInstance = ConstructorWithFilepath_Native(filepath);
        }

        internal Texture2D(IntPtr outInternalPtr)
        {
            m_UnmanagedInstance = outInternalPtr;
            Console.WriteLine("Created texture! Constructor3 (Internal) - " + outInternalPtr);
        }

        ~Texture2D()
        {
            Console.WriteLine("Destroyed texture!");
            Destructor_Native(m_UnmanagedInstance);
        }

        public void SetData(Vector4[] data)
        {
            SetData_Native(m_UnmanagedInstance, data, data.Length);
        }

        internal IntPtr m_UnmanagedInstance;

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr Constructor_Native(uint width, uint height);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr ConstructorWithFilepath_Native(string filepath);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Destructor_Native(IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetData_Native(IntPtr unmanagedInstance, Vector4[] data, int size);
    }
}
