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
        public float m_Distance = -4.0f;

        private Vector3 m_FocusPoint = new Vector3(0.0f);

        protected override void OnCreate()
        {
            Input.SetCursorMode(CursorMode.Locked);


        }


        private Vector2 m_OrbitAngles = new Vector2(0f, 0f);
        private Vector2 m_OrbitAnglesNoDelta = new Vector2(0f, 0f);

        protected override void OnUpdate(float deltaTime)
        {
            //this.Translation = ActorEntity.Translation + m_CameraOffset;

            if (Input.IsKeyPressed(KeyCode.Escape) && Input.GetCursorMode() == CursorMode.Locked)
                Input.SetCursorMode(CursorMode.Normal);

            UpdateFocusPoint(deltaTime);
            ManualRotation(deltaTime);

            Quaternion lookRotation = new Quaternion(new Vector3(m_OrbitAngles.Y * (Mathf.PI / 180.0f), m_OrbitAngles.X * (Mathf.PI / 180.0f), 0.0f));
            //Vector3 lookDirection = Transform.Transform.Forward;
            Vector3 lookDirection = lookRotation * Vector3.Forward;
            Vector3 lookPosition = m_FocusPoint - lookDirection * m_Distance;

            RaycastData raycastData = new RaycastData();
            raycastData.MaxDistance = m_Distance;
            raycastData.Origin = m_FocusPoint - lookDirection * 1.7f;
            raycastData.Direction = -lookDirection;
            RaycastHit raycastHitData = new RaycastHit();
            if(Physics.Raycast(raycastData, out raycastHitData))
            {
                this.Translation = raycastHitData.Position + lookDirection * 0.5f;
            }
            else
            {
                this.Translation = lookPosition;
            }

            //Transform transform = new Transform();
            //transform.LookAt(m_FocusPoint, lookDirection * m_Distance);

            this.Rotation = -new Vector3(m_OrbitAngles.X, m_OrbitAngles.Y, 0.0f);
            
            //this.Translation = m_FocusPoint - this.Transform.Transform.Forward * m_Distance;
            //this.Rotation = (lookRotation * new Vector3(1.0f)) * (180.0f / Mathf.PI);

            //Log.Info("{0}, {1}, {2}", normalzedLookPosition.X, normalzedLookPosition.Y, normalzedLookPosition.Z);
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

        private float m_FocusRadius = 0.0f;
        private float m_FocusCentering = 0.1f;

        void UpdateFocusPoint(float deltaTime)
        {
            Vector3 targetPoint = ActorEntity.Translation;
            if (m_FocusRadius > 0f)
            {
                

                float distance = Vector3.Distance(targetPoint, m_FocusPoint);

                float t = 1f;
                if (distance > 0.01f && m_FocusCentering > 0f)
                {
                    t = (float)Math.Pow(1.0f - m_FocusCentering, deltaTime);
                }

                if (distance > m_FocusRadius)
                {
                    t = Math.Min(t, m_FocusRadius / distance);
                }
                m_FocusPoint = Vector3.Lerp(targetPoint, m_FocusPoint, t);
            }
            else
            {
                //focusPoint = targetPoint;
                m_FocusPoint = targetPoint;
            }
        }

        Vector2 m_LastInput = new Vector2(0.0f);
        void ManualRotation(float deltaTime)
        {
            //if (!Input.IsMouseButtonPressed(MouseButton.Button1)) return;

            Vector2 input = new Vector2(
                Input.GetMousePosition().X,
                Input.GetMousePosition().Y
           );
            Vector2 deltaInput = input - m_LastInput;
            m_LastInput = input;

            const float e = 0.001f;
            const float rotationSpeed = 5.0f;
            if (deltaInput.X < -e || deltaInput.X > e || deltaInput.Y < -e || deltaInput.Y > e)
            {
                m_OrbitAngles -= new Vector2(rotationSpeed * deltaInput.X * deltaTime, rotationSpeed * deltaInput.Y * deltaTime);
                //Log.Info("{0}, {1}", m_OrbitAngles.X, m_OrbitAngles.Y);
            }

        }

        protected override void OnDestroy()
        {
        }

    }
}