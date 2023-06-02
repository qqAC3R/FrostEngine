using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Frost
{
    public class Entity
    {
        private Action<Entity> m_CollisionBeginCallbacks;
        private Action<Entity> m_CollisionEndCallbacks;
        private Action<Entity> m_TriggerBeginCallbacks;
        private Action<Entity> m_TriggerEndCallbacks;

        private TransformComponent m_TransformComponent = null;

        protected Entity() { ID = 0; }

        internal Entity(ulong id)
        {
            ID = id;
        }

        ~Entity()
        {
        }

        public ulong ID { get; private set; }
        public string Tag => GetComponent<TagComponent>().Tag;
        public TransformComponent Transform
        {
            get
            {
                if (m_TransformComponent == null)
                    m_TransformComponent = GetComponent<TransformComponent>();

                return m_TransformComponent;
            }
        }

        public Vector3 Translation
        {
            get => Transform.Translation;
            set => Transform.Translation = value;
        }

        public Vector3 Rotation
        {
            get => Transform.Rotation;
            set => Transform.Rotation = value;
        }

        public Vector3 Scale
        {
            get => Transform.Scale;
            set => Transform.Scale = value;
        }

        protected virtual void OnCreate() { }
        protected virtual void OnUpdate(float deltaTime) { }
        protected virtual void OnPhysicsUpdate(float deltaTime) { }
        protected virtual void OnDestroy() { }

        public Entity Parent
        {
            get => new Entity(GetParent_Native(ID));
            set => SetParent_Native(ID, value.ID);
        }

        public Entity[] Children
        {
            get => GetChildren_Native(ID);
        }

        public T CreateComponent<T>() where T : Component, new()
        {
            CreateComponent_Native(ID, typeof(T));
            T component = new T();
            component.Entity = this;
            return component;
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            return HasComponent_Native(ID, typeof(T));
        }

        public bool HasComponent(Type type)
        {
            return HasComponent_Native(ID, type);
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (HasComponent<T>())
            {
                T component = new T();
                component.Entity = this;
                return component;
            }
            return null;
        }

        public Entity FindEntityByTag(string tag)
        {
            ulong entityID = FindEntityByTag_Native(tag);
            if (entityID == 0)
                return null;

            return new Entity(entityID);
        }

        public Entity FindEntityByID(ulong entityID)
        {
            // TODO: Verify the entity id
            return new Entity(entityID);
        }

        public ulong GetID()
        {
            return ID;
        }

        static public Entity Create()
        {
            return new Entity(CreateEntity_Native());
        }

        // TODO: Instantiate method (for prefabs)?


        // Destroys the calling Entity
        public void Destroy()
        {
            DestroyEntity_Native(ID);
        }


        // Adding callbacks
        public void AddCollisionBeginCallback(Action<Entity> callback)
        {
            m_CollisionBeginCallbacks += callback;
        }

        public void AddCollisionEndCallback(Action<Entity> callback)
        {
            m_CollisionEndCallbacks += callback;
        }

        public void AddTriggerBeginCallback(Action<Entity> callback)
        {
            m_TriggerBeginCallbacks += callback;
        }

        public void AddTriggerEndCallback(Action<Entity> callback)
        {
            m_TriggerEndCallbacks += callback;
        }

        // Calling up callbacks
        private void OnCollisionBegin(ulong id)
        {
            if (m_CollisionBeginCallbacks != null)
                m_CollisionBeginCallbacks.Invoke(new Entity(id));
        }

        private void OnCollisionEnd(ulong id)
        {
            if (m_CollisionEndCallbacks != null)
                m_CollisionEndCallbacks.Invoke(new Entity(id));
        }

        private void OnTriggerBegin(ulong id)
        {
            if (m_TriggerBeginCallbacks != null)
                m_TriggerBeginCallbacks.Invoke(new Entity(id));
        }

        private void OnTriggerEnd(ulong id)
        {
            if (m_TriggerEndCallbacks != null)
                m_TriggerEndCallbacks.Invoke(new Entity(id));
        }



        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong GetParent_Native(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong SetParent_Native(ulong entityID, ulong parentID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity[] GetChildren_Native(ulong entityID);


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateComponent_Native(ulong entityID, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool HasComponent_Native(ulong entityID, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong FindEntityByTag_Native(string tag);


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong CreateEntity_Native();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong DestroyEntity_Native(ulong entityID);
    }
}
