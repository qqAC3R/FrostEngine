using System;
using System.Threading;
using Frost;

namespace Frost
{
    public class ActorCamera : Entity
    {
        public Entity ActorEntity = null;
        //public Entity ActorEntityMesh = null;
        public Vector3 m_CameraOffset = new Vector3(0, 1, 3);
        public float m_Distance = 8.0f;

        protected override void OnCreate()
        {
            Input.SetCursorMode(CursorMode.Locked);


        }
        protected override void OnUpdate(float deltaTime)
        {
            this.Translation = ActorEntity.Translation + m_CameraOffset;

            if (Input.IsKeyPressed(KeyCode.Escape) && Input.GetCursorMode() == CursorMode.Locked)
                Input.SetCursorMode(CursorMode.Normal);
#if false
            ManualRotation();

            Quaternion lookRotation = new Quaternion(orbitAngles);

            Vector3 focusPoint = ActorEntity.Translation;
            //Vector3 lookDirection = ActorEntityMesh.GetComponent<TransformComponent>().WorldTransform.Forward;
            //Vector3 lookDirection = ActorEntity.Transform.Transform.Forward;
            Vector3 lookDirection = lookRotation * Vector3.Forward;

            Vector3 lookPosition = focusPoint - lookDirection * m_Distance;
            this.Transform.Translation = lookPosition;
            this.Transform.Rotation = lookDirection;

            //this.Translation = focusPoint - lookDirection * m_Distance;
#endif

            //m_Distance

            //this.Rotation = new Vector3(this.Rotation.X, 0.0f, this.Rotation.Z);
        }

#if false
        void ManualRotation()
        {
            Vector2 input = new Vector2(
                Input.GetMousePosition().X,
                Input.GetMousePosition().Y
            );
            const float e = 0.001f;
            const float rotationSpeed = 20.0f;
            if (input.X < -e || input.X > e || input.Y < -e || input.Y > e)
            {
                orbitAngles += rotationSpeed * input;
            }
        }
#endif

        protected override void OnDestroy()
        {
        }

    }
}