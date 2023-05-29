using System;
using System.Security.Policy;
using Frost;

namespace Frost
{
    public class Actor : Entity
    {
        //public Vector3 m_Abledo = new Vector3(1, 1, 1);
        //public float m_Roughness = 0.2f;
        //public float m_Metalness = 1.0f;
        //public float m_Emission = 0.0f;

        public float m_MovementForce = 5.0f;

        //private Texture2D m_TextureField;
        //private Mesh m_Mesh;

        private Vector3 m_LastPosition = new Vector3(0.0f);
        private string[] m_Animations;

        protected override void OnCreate()
        {
            //m_TextureField = new Texture2D("SandboxProject/Assets/Textures/fields.jpg");
            //m_Mesh = new Mesh("SandboxProject/Assets/Meshes/Torus.fbx");

            //GetComponent<RigidBodyComponent>().SetLockFlag(ActorLockFlag.RotationXYZ, true);
            //GetComponent<RigidBodyComponent>().SetLockFlag(ActorLockFlag.RotationXYZ, true);

            m_Animations = Children[0].GetComponent<AnimationComponent>().Animations;
            //foreach (var animation in m_Animations)
            //{
            //    Log.Info(animation);
            //}
        }


        protected override void OnUpdate(float deltaTime)
        {
#if false
            GetComponent<MeshComponent>().GetMaterial(0).AlbedoColor = m_Abledo;
            GetComponent<MeshComponent>().GetMaterial(0).Roughness = m_Roughness;
            GetComponent<MeshComponent>().GetMaterial(0).Metalness = m_Metalness;
            GetComponent<MeshComponent>().GetMaterial(0).Emission = m_Emission;
#endif
            m_Animations = Children[0].GetComponent<AnimationComponent>().Animations;
            if (Translation == m_LastPosition)
            {
                Children[0].GetComponent<AnimationComponent>().SetActiveAnimation(m_Animations[2]);
            }
            else if ((m_LastPosition.Y - Translation.Y) > 0.02f)
            {
                Children[0].GetComponent<AnimationComponent>().SetActiveAnimation(m_Animations[1]);
                m_LastPosition = Translation;
            }
            else
            {
                Children[0].GetComponent<AnimationComponent>().SetActiveAnimation(m_Animations[0]);
                m_LastPosition = Translation;
            }


            if (Input.IsKeyPressed(KeyCode.A))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(0, 0, -m_MovementForce * deltaTime), ForceMode.Force);

                Children[0].Rotation = Vector3.Lerp(Children[0].Rotation, new Vector3(90.0f, 180.0f, 0), 0.1f);
            }
            if(Input.IsKeyPressed(KeyCode.D))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(0, 0, m_MovementForce * deltaTime), ForceMode.Force);
                Children[0].Rotation = Vector3.Lerp(Children[0].Rotation, new Vector3(90.0f, 0.0f, 0), 0.1f);
            }
            if (Input.IsKeyPressed(KeyCode.S))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(-m_MovementForce * deltaTime, 0, 0), ForceMode.Force);
                Children[0].Rotation = Vector3.Lerp(Children[0].Rotation, new Vector3(90.0f, 270.0f, 0), 0.1f);
            }
            if (Input.IsKeyPressed(KeyCode.W))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(m_MovementForce * deltaTime, 0, 0), ForceMode.Force);
                Children[0].Rotation = Vector3.Lerp(Children[0].Rotation, new Vector3(90.0f, 90.0f, 0), 0.1f);
            }

            if (Input.IsKeyPressed(KeyCode.Space))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(0, 2000.0f * deltaTime, 0), ForceMode.Force);
            }

        }
    }


}