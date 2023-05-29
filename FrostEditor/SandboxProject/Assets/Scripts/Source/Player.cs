using System;
using Frost;

namespace Frost
{
    public class Player : Entity
    {
        //public int m_VarInt = 5;
        //public float m_VarFloat = 5.0f;
        public float m_CameraFOV = 80.0f;
        //public uint m_VarUInt = 52;
        //public Vector2 m_VarVec2 = new Vector2(2.0f, 1.0f);
        //public Vector3 m_VarVec3 = new Vector3(0.0f, 1.0f, 2.0f);
        //public Vector4 m_VarVec4 = new Vector4(1.0f, 1.0f, 2.0f, 5.0f);
        public Texture2D m_Texture;
        //public Entity m_CameraEntity = null;
        public string m_VarStr = "Hello world!!";

        //public Entity m_Cube = null;
        public Vector3 m_CameraOffset = new Vector3(0, 1, 3);

        public float m_VarProperty
        {
            get;
            set;
        }

        protected override void OnCreate()
        {
            //m_Texture = new Texture2D(3, 1);
            //Console.WriteLine("Created texture!");

            //Vector4[] image = { new Vector4(1.0f), new Vector4(0.0f), new Vector4(1.0f) };
            //m_Texture.SetData(image);


            //ulong parentId = Parent.GetID();

            //Console.WriteLine($"C#: Hi : {m_VarProperty}");
            //Console.WriteLine($"C#: Parent ID: {parentId}");

            //foreach(var child in Children)
            //{
            //    Console.WriteLine($"C#: Children ID: {child.GetID()}");
            //
            //    //Console.WriteLine($"C#: Has Children Transform Component: {child.HasComponent<TransformComponent>()}");
            //    child.CreateComponent<MeshComponent>();
            //
            //
            //}
            //Console.WriteLine($"C#: Cube Entity ID: {FindEntityByTag("Cube").ID}");
            //Console.WriteLine($"C#: Does Cube Entity have Mesh: {FindEntityByTag("Cube").CreateComponent<MeshComponent>()}");

            //Entity ent = Entity.Create();
            //Console.WriteLine($"C#: Created Entity: {ent.ID}");

            //Texture2D texture = new Texture2D("Resources/Scenes/fields.jpg");
            //Texture2D texture = new Texture2D(100, 200);

#if false
            TransformComponent t = GetComponent<TransformComponent>();
            Console.WriteLine($"Translation: {t.Translation.X}, {t.Translation.Y}, {t.Translation.Z}");
            Console.WriteLine($"Rotation: {t.Rotation.X}, {t.Rotation.Y}, {t.Rotation.Z}");
            Console.WriteLine($"Scale: {t.Scale.X}, {t.Scale.Y}, {t.Scale.Z}");
            Console.WriteLine($"Global Translation: {t.WorldTransform.Position.X}, {t.WorldTransform.Position.Y}, {t.WorldTransform.Position.Z}");


            t.Translation = new Vector3(0.0f, 1.0f, 6.0f);
            t.Rotation = new Vector3(0.0f, 1.0f, 0.0f);
            t.Scale = new Vector3(2.0f, 2.0f, 2.0f);

            Console.WriteLine($"Translation: {t.Translation.X}, {t.Translation.Y}, {t.Translation.Z}");
            Console.WriteLine($"Rotation: {t.Rotation.X}, {t.Rotation.Y}, {t.Rotation.Z}");
            Console.WriteLine($"Scale: {t.Scale.X}, {t.Scale.Y}, {t.Scale.Z}");
            Console.WriteLine($"Global Translation: {t.WorldTransform.Position.X}, {t.WorldTransform.Position.Y}, {t.WorldTransform.Position.Z}");
#endif


        }
        protected override void OnUpdate(float deltaTime)
        {
            GetComponent<CameraComponent>().FOV = m_CameraFOV;
            //Log.Critical("N2gga3");
            //this.Translation = m_Cube.Translation + m_CameraOffset;

            //Console.WriteLine($"Property: {m_VarProperty}, Int: {m_VarInt}, Float: {m_VarFloat}, UINT: {m_VarUInt}");
            //Console.WriteLine($"Vec2: {m_VarVec2.X}, {m_VarVec2.Y}");
            //Console.WriteLine($"Vec3: {m_VarVec3.X}, {m_VarVec3.Y}, {m_VarVec3.Z}");
            //Console.WriteLine($"Vec4: {m_VarVec4.X}, {m_VarVec4.Y}, {m_VarVec4.Z}, {m_VarVec4.W}");
            //if(m_CameraEntity != null)
            //{
            //    Console.WriteLine($"{m_CameraEntity.Translation.ToString()}");
            //}
            //Console.WriteLine($"{m_VarStr}");
        }

        protected override void OnDestroy()
        {
            //m_Texture.Destroy();
        }

    }


}