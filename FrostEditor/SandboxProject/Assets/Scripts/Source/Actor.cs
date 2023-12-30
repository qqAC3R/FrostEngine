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

        public Entity m_MeshChild;
        public Entity m_CameraController;

        public Prefab m_SphereBullet = null;

        //private Texture2D m_TextureField;
        //private Mesh m_Mesh;

        private Vector3 m_LastPosition = new Vector3(0.0f);
        //private string[] m_Animations;

        private float m_Acceleration = 0.0f;
        private bool m_IsCollided = false;

        protected override void OnCreate()
        {
            //m_TextureField = new Texture2D("SandboxProject/Assets/Textures/fields.jpg");
            //m_Mesh = new Mesh("SandboxProject/Assets/Meshes/Torus.fbx");

            //GetComponent<RigidBodyComponent>().SetLockFlag(ActorLockFlag.RotationXYZ, true);
            //GetComponent<RigidBodyComponent>().SetLockFlag(ActorLockFlag.RotationXYZ, true);

            //m_Animations = Children[0].GetComponent<AnimationComponent>().Animations;

            //m_CameraController = FindEntityByTag("Camera");


            AddCollisionBeginCallback(o => { m_IsCollided = true; });
            AddCollisionEndCallback(o => { m_IsCollided = false; });
        }

        protected override void OnUpdate(float deltaTime)
        {

            Vector3 velocity = GetComponent<RigidBodyComponent>().LinearVelocity;
            Vector2 movingVelocity = new Vector2(velocity.X, velocity.Z);
            float acceleration = movingVelocity.Length();
            Children[0].GetComponent<AnimationComponent>().SetFloatInput("Acceleration", acceleration);

            float velocityY = (-velocity.Y) / 4.0f;
            Children[0].GetComponent<AnimationComponent>().SetFloatInput("FallingSpeed", velocityY);


            if (Input.IsKeyPressed(KeyCode.A))
            {
                Vector3 cameraDirection = m_CameraController.Rotation;
                cameraDirection.X -= 90.0f;
                //Quaternion lookRotation = new Quaternion(cameraDirection * (Mathf.PI / 180.0f));
                //Vector3 cameraForwardVector = lookRotation * Vector3.Forward;

                Vector3 finalRotation = new Vector3(0.0f, -cameraDirection.X + 180.0f, 0.0f);

                if (Input.IsKeyPressed(KeyCode.S))
                    finalRotation = new Vector3(0.0f, -cameraDirection.X - 180.0f, 0.0f);

                m_MeshChild.Rotation = Vector3.Lerp(m_MeshChild.Rotation, finalRotation, 0.1f);

                float sinX = (float)Math.Sin(finalRotation.Y * (Mathf.PI / 180.0f));
                float cosX = (float)Math.Cos(finalRotation.Y * (Mathf.PI / 180.0f));
                Vector3 force = new Vector3(sinX * m_MovementForce * deltaTime, 0.0f, cosX * m_MovementForce * deltaTime);
                GetComponent<RigidBodyComponent>().AddForce(force, ForceMode.Force);

            }
            if(Input.IsKeyPressed(KeyCode.D))
            {
                Vector3 cameraDirection = m_CameraController.Rotation;
                cameraDirection.X += 90.0f;
                //Quaternion lookRotation = new Quaternion(cameraDirection * (Mathf.PI / 180.0f));
                //Vector3 cameraForwardVector = lookRotation * Vector3.Forward;

                Vector3 finalRotation = new Vector3(0.0f, 180.0f - cameraDirection.X, 0.0f);
                m_MeshChild.Rotation = Vector3.Lerp(m_MeshChild.Rotation, finalRotation, 0.1f);

                float sinX = (float)Math.Sin(finalRotation.Y * (Mathf.PI / 180.0f));
                float cosX = (float)Math.Cos(finalRotation.Y * (Mathf.PI / 180.0f));
                Vector3 force = new Vector3(sinX * m_MovementForce * deltaTime, 0.0f, cosX * m_MovementForce * deltaTime);
                GetComponent<RigidBodyComponent>().AddForce(force, ForceMode.Force);

            }
            if (Input.IsKeyPressed(KeyCode.S))
            {
                Vector3 cameraDirection = m_CameraController.Rotation;
                //Quaternion lookRotation = new Quaternion(cameraDirection * (Mathf.PI / 180.0f));
                //Vector3 cameraForwardVector = lookRotation * Vector3.Forward;

                Vector3 finalRotation = new Vector3(0.0f, -cameraDirection.X, 0.0f);
                //m_MeshChild.Rotation = finalRotation;
                m_MeshChild.Rotation = Vector3.Lerp(m_MeshChild.Rotation, finalRotation, 0.1f);

                float sinX = (float)Math.Sin(finalRotation.Y * (Mathf.PI / 180.0f));
                float cosX = (float)Math.Cos(finalRotation.Y * (Mathf.PI / 180.0f));
                Vector3 force = new Vector3(sinX * m_MovementForce * deltaTime, 0.0f, cosX * m_MovementForce * deltaTime);
                GetComponent<RigidBodyComponent>().AddForce(force, ForceMode.Force);

            }
            if (Input.IsKeyPressed(KeyCode.W))
            {
                Vector3 cameraDirection = m_CameraController.Rotation;
                //Quaternion lookRotation = new Quaternion(cameraDirection * (Mathf.PI / 180.0f));
                //Vector3 cameraForwardVector = lookRotation * Vector3.Forward;

                Vector3 finalRotation = new Vector3(0.0f, 180.0f - cameraDirection.X, 0.0f);
                m_MeshChild.Rotation = Vector3.Lerp(m_MeshChild.Rotation, finalRotation, 0.1f);

                float sinX = (float)Math.Sin(finalRotation.Y * (Mathf.PI / 180.0f));
                float cosX = (float)Math.Cos(finalRotation.Y * (Mathf.PI / 180.0f));
                Vector3 force = new Vector3(sinX * m_MovementForce * deltaTime, 0.0f, cosX * m_MovementForce * deltaTime);
                GetComponent<RigidBodyComponent>().AddForce(force, ForceMode.Force);
            }

            if (Input.IsKeyPressed(KeyCode.Space))
            {
                GetComponent<RigidBodyComponent>().AddForce(new Vector3(0, 2000.0f * deltaTime, 0), ForceMode.Force);
            }

#if false
            GetComponent<MeshComponent>().GetMaterial(0).AlbedoColor = m_Abledo;
            GetComponent<MeshComponent>().GetMaterial(0).Roughness = m_Roughness;
            GetComponent<MeshComponent>().GetMaterial(0).Metalness = m_Metalness;
            GetComponent<MeshComponent>().GetMaterial(0).Emission = m_Emission;
#endif
        }
    }


}